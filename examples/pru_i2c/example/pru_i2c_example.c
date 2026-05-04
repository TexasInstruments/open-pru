/*
 *  Copyright (C) 2024 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/pruicss.h>
#include <drivers/pinmux.h>
#include <pru0_load_bin.h>
#include <pru_i2c_example.h>
//#include <driver/include/i2c_driver.h>

/*
 *  This is an example project for PRU I2C
 * 
 */

/*============================================================================*/
/*                            Macros                                          */
/*============================================================================*/

/* ========================================================================== */
/*                       Function Declarations                                */
/* ========================================================================== */

void I2c_pruicssInit(void);


/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */


/** \brief Global Structure pointer holding PRUSS1 memory Map. */

PRUICSS_Handle gPruIcssXHandle;
//extern PRUICSS_IntcInitData icss0_intc_initdata;
/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void I2c_pruicssInit(void)
{
    int status;
    gPruIcssXHandle = PRUICSS_open(CONFIG_PRU_ICSS0);
    /* PRUICSS_PRUx holds value 0 or 1 depending on whether we are using PRU0 or PRU1 slice */
    status = PRUICSS_initMemory(gPruIcssXHandle, PRUICSS_DATARAM(PRUICSS_PRU0));
    DebugP_assert(status != 0);
    
    status = PRUICSS_loadFirmware(gPruIcssXHandle, PRUICSS_PRU0, PRU0Firmware_0, sizeof(PRU0Firmware_0));
    DebugP_assert(SystemP_SUCCESS == status);

}
#ifdef SOC_AM261X
static Pinmux_PerCfg_t gPinMuxMainDomainCfg1[] = {
     /* PRU-ICSS1 pin config */
     /* PR1_PRU0_GPIO2 -> GPIO83 (B4) */
     {
         PIN_GPIO83,
         ( PIN_MODE(4) | PIN_PULL_DISABLE | PIN_SLEW_RATE_LOW )
     },
     /* PRU-ICSS1 pin config */
     /* PR1_PRU0_GPIO1 -> GPIO82 (A4) */
     {
         PIN_GPIO82,
         ( PIN_MODE(4) | PIN_PULL_DISABLE | PIN_SLEW_RATE_LOW )
     },
     /* PRU-ICSS1 pin config */
     /* PR1_PRU0_GPIO0 -> GPIO81 (B5) */
     {
         PIN_GPIO81,
         ( PIN_MODE(4) | PIN_PULL_DISABLE | PIN_SLEW_RATE_LOW )
     },
     /* PRU-ICSS1 pin config */
     /* PR1_PRU0_GPIO9 -> GPIO80 (A5) */
     {
         PIN_GPIO80,
         ( PIN_MODE(4) | PIN_PULL_DISABLE | PIN_SLEW_RATE_LOW )
     },


    {PINMUX_END, PINMUX_END}
};


/*
 * Pinmux
 */

void Pinmux_init1(void)
{
    Pinmux_config(gPinMuxMainDomainCfg1, PINMUX_DOMAIN_ID_MAIN);


}
#endif
void pru_i2c_main(void *args)
{
    Drivers_open(); // check return status

    int status;
    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);
#ifdef SOC_AM261X
    Pinmux_init1();
#endif
    I2c_pruicssInit();

   while(1)
   {

   }

    Board_driversClose();
    Drivers_close();

    return;
}
