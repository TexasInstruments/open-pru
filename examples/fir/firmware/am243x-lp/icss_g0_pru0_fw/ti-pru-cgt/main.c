/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

/**
 *  \file   main.c
 *
 *  \brief  File that calls the FIR filter
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <drivers/pruicss.h>
#include <drivers/hw_include/hw_types.h>
#include "am243_am64_pru_pinmux.h"
#include <drivers/hw_include/am64x_am243x/cslr_soc_baseaddress.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_ctrl_mmr.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_pll_mmr.h>
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
/*                          External Function Declarations                    */
/* ========================================================================== */
void FN_FIR_64();
/* ========================================================================== */
/*                          Function Defenitions                              */
/* ========================================================================== */

/*
 * Function : double_to_q31
 *
 * Description:
 *       Converts double to Q31 format taking care of rounding and
 *       saturation issues.
 */

static inline int32_t double_to_q31(double val)
{
    /* Clamp the input value */
    if (val >= 1.0)
        val = 0.9999999999999999;
    else if (val <= -1.0)
        val = -1.0;

    /* Scale to Q31 */
    double scaled = val * FIR_TWO_RAISED_TO_31;

    /* Round to nearest integer */
    int64_t rounded;
    if (scaled >= 0.0)
        rounded = (int64_t)(scaled + 0.5);   /* round‑up for .5 and above */
    else
        rounded = (int64_t)(scaled - 0.5);   /* round‑down for .5 and above */

    /* Saturate to the Q31 limits */
    const int64_t Q31_MAX =  0x7FFFFFFFLL;
    const int64_t Q31_MIN = -0x80000000LL;

    if (rounded > Q31_MAX) rounded = Q31_MAX;
    if (rounded < Q31_MIN) rounded = Q31_MIN;

    return (int32_t)rounded;
}
/*
 * void main(void)
 */
int main(void)
{
    volatile uint8_t coef_count = 0;
    /*  ----------------- clock config --------------------------  */
    /*
    ; core clock options:
    ; ICSSG0_CLKMUX_SEL: bit 0 = 1 PLL0, bit 0 = 0 PLL2
    ; MAIN_PLL0_HSDIV9: 2 (333 MHz), 3 (250 MHz)
    ; MAIN_PLL2_HSDIV2: 5 (300 MHz), 7 (225 MHz)
    */
    /* configure ICSS clocks */
    /* unlock CTRLMMR config register partition 2 */
    HW_WR_REG32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_LOCK2_KICK0), PAD_KEY0);
    HW_WR_REG32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_LOCK2_KICK1), PAD_KEY1);
    /* ICSS_G0_CLKMUX_SEL:
      1h - MAIN_PLL0_HSDIV9_CLKOUT
    */
    HW_WR_REG32(CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_ICSSG0_CLKSEL, 0x00010001) ;
    /* unlock PLL0 and PLL2 ctrl register*/
    HW_WR_REG32(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL0_LOCKKEY0, PAD_KEY0);
    HW_WR_REG32(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL0_LOCKKEY1, PAD_KEY1);
    HW_WR_REG32(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL2_LOCKKEY0, PAD_KEY0);
    HW_WR_REG32(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL2_LOCKKEY1, PAD_KEY1);
    /* set MAIN_PLL0_HSDIV9 to 2 for core_clock =333MHz (3 for 250 MHz, 1 = 500 MHz, 9 = 100 MHz)*/
    HW_WR_REG8(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL0_HSDIV_CTRL9, 2);

    /* configure Scratch Pad Shift enable using ICSSG_SPP_REG MMR */
    asm("    ldi32 r11, 0x30026034");   /* load MII_G_RT_FDB_GEN_CFG2 MMR address */
    asm("    ldi32 r12, 0x2");          /* XFR_SHIFT_EN = 1 */
    asm("    sbbo &r12, r11, 0, 4");    /* write configuration to MMR */

    /* populate FIR coefficients in MEM*/
    fir_coef    = (int32_t*)FIR_COEF_MEM; /*point to the location of the FFT output data */
    while (coef_count < 64U)
    {
        fir_coef[coef_count] = double_to_q31(fir_filter_taps[coef_count]);
        coef_count++;
    }

    /* call FIR function */
    FN_FIR_64();

    __halt();
}
