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
 *  \brief  File that calls the configurable frequency generator function
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <drivers/pruicss.h>
#include <drivers/hw_include/hw_types.h>
#include "AM243_AM64_PRU_pinmux.h"
#include <drivers/hw_include/am64x_am243x/cslr_soc_baseaddress.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_ctrl_mmr.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_pll_mmr.h>
/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
volatile register unsigned int __R30; // output register
volatile register unsigned int __R31; // input register
/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
void CONFIG_FREQ_GEN();

/*
 * void main(void)
 */
int main(void)
{
    /* ---------- pin-mux configuration --------------------- */
    /* unlock PADMMR config register partition 0 */
    HW_WR_REG32(PAD_PARTITION0_L0, PAD_KEY0);
    HW_WR_REG32(PAD_PARTITION0_L1, PAD_KEY1);
    /* unlock PADMMR config register partition 1 */
    HW_WR_REG32(PAD_PARTITION1_L0, PAD_KEY0);
    HW_WR_REG32(PAD_PARTITION1_L1, PAD_KEY1);
    /* FG_OUTPUT  - PRG0_PRU0_GPI0 - BP.33*/
    HW_WR_REG32(PRG0_PRU0_GPO0, PRU_GPO);
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
    /* call assembler routine to generate output pattern */
    CONFIG_FREQ_GEN();
    return(0);
}
