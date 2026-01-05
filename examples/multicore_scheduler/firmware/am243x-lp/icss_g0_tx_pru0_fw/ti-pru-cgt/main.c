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
 *  \brief  Multi-core scheduling configuration and code for TXPRU0
 */
/**
 *  \file   main.c
 *
 *  \brief  Multi-core scheduling configuration and code for PRU1
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
void FN_PWM_INIT_TOGGLE_TXPRU0_TASK();
/* ========================================================================== */
/*                          Macros                                            */
/* ========================================================================== */
#define TM_REG_BASE_TXPRU0 CSL_ICSS_G_PR1_TASKS_MGR_PRU_TX0_PR1_TASKS_MGR_PRU_TX0_MMR_REGS_BASE
#define TASKS_MGR_GLOBAL_CFG   0x00
#define TASKS_MGR_TS2_PC_S0    0x1C

/*
 * void main()
 */
void main(void)
{


/* ---------- task manager configuration ------------------- */
    /* disable task manager */
    asm("   tsen 0");
    /* clear event from previous debug.*/
    HW_WR_REG32((TM_REG_BASE_TXPRU0+TASKS_MGR_GLOBAL_CFG), 0x0fff);
    asm("   xin     252, &r0.b3,1");
    asm("   nop ");
    asm("   nop ");
    HW_WR_REG32((TM_REG_BASE_TXPRU0+TASKS_MGR_GLOBAL_CFG), 0x0000);
    /* configure task manager, TS2 is highest priority and can pre-empt TS1
    *  general purpose mode = 2 ,
    *  TS2_S0 task enable  (bit 7)
    */
    HW_WR_REG32((TM_REG_BASE_TXPRU0+TASKS_MGR_GLOBAL_CFG), 0x0082);
    /* set address of tasks in TS2_0*/
    HW_WR_REG32((TM_REG_BASE_TXPRU0+TASKS_MGR_TS2_PC_S0), (unsigned int)FN_PWM_INIT_TOGGLE_TXPRU0_TASK);
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
