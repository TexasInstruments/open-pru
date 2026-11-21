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
#if defined(PRU_GPIO_VIA_IOEXP)
#include <board/ioexp/ioexp_tca6424.h>
#endif

/*
 *  The R5F loads the spinlock firmware onto PRU0 and PRU1 of ICSSG0. Each PRU
 *  drives a debug GPO while it holds the lock; the pinmux for both pins is in
 *  example.syscfg, and readme.md lists the header pins.
 */

PRUICSS_Handle gPruIcss0Handle;

/*
 *  On the AM64x and AM243x EVMs the PRU GPIOs only reach the expansion header
 *  once the TCA6424 I/O expander enables them. Those boards' makefiles define
 *  PRU_GPIO_VIA_IOEXP. The AM243x LaunchPad has no such switch.
 */
#if defined(PRU_GPIO_VIA_IOEXP)

static TCA6424_Config gTCA6424_Config;

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
    DebugP_assert(status == SystemP_SUCCESS);
}

#endif

void spinlock_c_main(void *args)
{
    Drivers_open(); // check return status

    int status;
    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);

    gPruIcss0Handle = PRUICSS_open(CONFIG_PRU_ICSS0);

    #if defined(PRU_GPIO_VIA_IOEXP)
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
