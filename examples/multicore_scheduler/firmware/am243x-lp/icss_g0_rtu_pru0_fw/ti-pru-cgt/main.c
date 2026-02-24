/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

/**
 *  \file   main.c
 *
 *  \brief  Multi-core scheduling configuration and code for RTU0
 */
/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdint.h>
#include <drivers/pruicss.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/pruicss/g_v0/cslr_icss_g.h>
/* ========================================================================== */
/*                          External Function Declarations                    */
/* ========================================================================== */
void FN_PWM_INIT_TOGGLE_RTU0_TASK();
/* ========================================================================== */
/*                          Macros                                            */
/* ========================================================================== */
#define TM_REG_BASE_RTU0 CSL_ICSS_G_PR1_TASKS_MGR_RTU0_PR1_TASKS_MGR_RTU0_MMR_REGS_BASE
#define TASKS_MGR_GLOBAL_CFG   0x00
#define TASKS_MGR_TS2_PC_S0    0x1C

/*
 * void main()
 */
void main(void)
{
/* Note: Event mapping and timer configuration 
*  for all cores are done from PRU0 in this project
*  to keep the configurations in one place and 
*  make any changes to the configuration easier.
*/
/* ---------- task manager configuration ------------------- */
    /* disable task manager */
    asm("   tsen 0");
    /* clear event from previous debug.*/
    HW_WR_REG32((TM_REG_BASE_RTU0+TASKS_MGR_GLOBAL_CFG), 0x0fff);
    asm("   xin     252, &r0.b3,1");
    asm("   nop ");
    asm("   nop ");
    HW_WR_REG32((TM_REG_BASE_RTU0+TASKS_MGR_GLOBAL_CFG), 0x0000);
    /* configure task manager, TS2 is highest priority and can pre-empt TS1
    *  general purpose mode = 2 ,
    *  TS2_S0 task enable  (bit 7)
    */
    HW_WR_REG32((TM_REG_BASE_RTU0+TASKS_MGR_GLOBAL_CFG), 0x0082);
    /* set address of tasks in TS2_0*/
    HW_WR_REG32((TM_REG_BASE_RTU0+TASKS_MGR_TS2_PC_S0), (unsigned int) FN_PWM_INIT_TOGGLE_RTU0_TASK);
    /* pre load for toggling
    *  toggling value for pwm pin (toggling sets POS_INIT and NEG_INT to 0 or 1)
    *  r24.b2 --> Set PIN high
    *  r24.b3 --> Set PIN low
    */
    asm("   ldi32 r24, 0x050a8000");
    /* enable task manager*/
    asm("   tsen 1");
    /* idle loop */
    while(1)
    {
    }
}
