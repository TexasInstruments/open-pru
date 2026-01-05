/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

/**
 *  \file   main.c
 *
 *  \brief  Multi-core scheduling configuration and code for PRU0
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdint.h>
#include <drivers/pruicss.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/hw_include/am64x_am243x/cslr_soc_baseaddress.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_ctrl_mmr.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_pll_mmr.h>
#include "am243_am64_pru_pinmux.h"

/* ========================================================================== */
/*                          External Function Declarations                    */
/* ========================================================================== */
void FN_MCS_CFG();
/*
 * void main()
 */
void main(void)
{

    // disable task manager
    asm("   tsen 0");

/* ---------- PRU clock and IEP clock configuration -------------- */
    /* unlock CTRLMMR config register partition 2 */
    HW_WR_REG32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_LOCK2_KICK0), PAD_KEY0);
    HW_WR_REG32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_LOCK2_KICK1), PAD_KEY1);
    /*
    ; core clock options:
    ; ICSSG0_CLKMUX_SEL: bit 0 = 1 PLL0, bit 0 = 0 PLL2
    ; MAIN_PLL0_HSDIV9: 2 (333 MHz), 3 (250 MHz)
    ; MAIN_PLL2_HSDIV2: 5 (300 MHz), 7 (225 MHz)
    */
    /* bit 0: 1 selects MAIN_PLL0_HSDIV9 for core clock
     * bit 16: 1 selects MAIN_PLL0_HSDIV6 for iep clock
    */
    HW_WR_REG32(CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_ICSSG0_CLKSEL, 0x00010001) ;
    /* unlock PLL0 ctrl register */
    HW_WR_REG32(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL0_LOCKKEY0, PAD_KEY0);
    HW_WR_REG32(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL0_LOCKKEY1, PAD_KEY1);
    /* set MAIN_PLL0_HSDIV9 to 3 for core_clock = 250 MHz */
    HW_WR_REG8(CSL_PLL0_CFG_BASE + CSL_MAIN_PLL_MMR_CFG_PLL0_HSDIV_CTRL9, 0x03);
    /* Note : IEP timer is set to sync mode in FN_MCS_CFG() 
    *  later so IEP timer uses the set core clock
    */
/* ---------- pin-mux configuration --------------------- */
    /* unlock PADMMR config register partition 0 */
    HW_WR_REG32(PAD_PARTITION0_L0, PAD_KEY0);
    HW_WR_REG32(PAD_PARTITION0_L1, PAD_KEY1);
    /* unlock PADMMR config register partition 1 */
    HW_WR_REG32(PAD_PARTITION1_L0, PAD_KEY0);
    HW_WR_REG32(PAD_PARTITION1_L1, PAD_KEY1);
    /*PWM2_0 on BP.31*/
    HW_WR_REG32(PRG0_PRU0_GPO2, 0x00010003);

/* ---------- additional configuration ------------------- */
    /*asssembly routine to configure IEP timer, compare registers and task manager*/
    FN_MCS_CFG();

/* ----------- end of configuration  ----------------------- */
    /* enable task manager */
    asm("   tsen 1");

    /* idle loop */
    while(1)
    {

    }
}

