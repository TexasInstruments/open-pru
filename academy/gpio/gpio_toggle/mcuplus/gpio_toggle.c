/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdio.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/pruicss.h>

#include <pru0_load_bin.h>
#include <pru1_load_bin.h>
#if defined(SOC_AM64X) || defined(SOC_AM243X)
#include <board/ioexp/ioexp_tca6424.h>
#endif
#if defined(SOC_AM263PX)
#include <board/ioexp/ioexp_tca6416.h>
#endif



/*
 *  This is an example project to show R5F
 *  loading PRU firmware.
 */

/** \brief Global Structure pointer holding PRUSS1 memory Map. */

PRUICSS_Handle gPruIcss0Handle;

#if defined(SOC_AM263PX)
static TCA6416_Config  gTCA6416_Config;

static void i2c_io_expander(void *args)
{
    int32_t             status = SystemP_SUCCESS;
    TCA6416_Params      tca6416Params;
    TCA6416_Params_init(&tca6416Params);
    status = TCA6416_open(&gTCA6416_Config, &tca6416Params);
    uint32_t            ioIndex;

    if(status == SystemP_SUCCESS)
    {
        /* set P07 high which controls AM263P_PRU_MUX_SEL -> enable PRU1 and PRU0 GPIOs for AM263Px */
        ioIndex = 0x07;
        status = TCA6416_setOutput(
                     &gTCA6416_Config,
                     ioIndex,
                     TCA6416_OUT_STATE_HIGH);

        /* Configure as output  */
        status += TCA6416_config(
                      &gTCA6416_Config,
                      ioIndex,
                      TCA6416_MODE_OUTPUT);
    }
    TCA6416_close(&gTCA6416_Config);
}

#endif

#if defined(SOC_AM64X) || defined(SOC_AM243X)

static TCA6424_Config  gTCA6424_Config;

static void i2c_io_expander(void *args)
{
    int32_t             status = SystemP_SUCCESS;
    TCA6424_Params      tca6424Params;
    TCA6424_Params_init(&tca6424Params);
    status = TCA6424_open(&gTCA6424_Config, &tca6424Params);
    uint32_t            ioIndex;

    if(status == SystemP_SUCCESS)
    {
        /* set P12 high which controls CPSW_FET_SEL -> enable PRU1 and PRU0 GPIOs */
        ioIndex = 0x0a;
        status = TCA6424_setOutput(
                     &gTCA6424_Config,
                     ioIndex,
                     TCA6424_OUT_STATE_HIGH);

        /* Configure as output  */
        status += TCA6424_config(
                      &gTCA6424_Config,
                      ioIndex,
                      TCA6424_MODE_OUTPUT);
    }
    TCA6424_close(&gTCA6424_Config);
}

#endif


void gpio_toggle_main(void *args)
{
     Drivers_open(); // check return status

     int status;
     status = Board_driversOpen();
     DebugP_assert(SystemP_SUCCESS == status);

     gPruIcss0Handle = PRUICSS_open(CONFIG_PRU_ICSS0);

     #if defined(SOC_AM263PX) || defined(SOC_AM64X) || defined(SOC_AM243X)
     /* Configure the IO Expander to connect the PRU IOs */
     i2c_io_expander(NULL);
     #endif

     status = PRUICSS_initMemory(gPruIcss0Handle, PRUICSS_DATARAM(PRUICSS_PRU0));
     DebugP_assert(status != 0);

     status = PRUICSS_initMemory(gPruIcss0Handle, PRUICSS_DATARAM(PRUICSS_PRU1));
     DebugP_assert(status != 0);

     status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_PRU0, PRU0Firmware_0, sizeof(PRU0Firmware_0));
     DebugP_assert(SystemP_SUCCESS == status);
     status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_PRU1, PRU1Firmware_0, sizeof(PRU1Firmware_0));
     DebugP_assert(SystemP_SUCCESS == status);

     while (1)
     {
        ClockP_usleep(1);
     }

     Board_driversClose();
     Drivers_close();
}
