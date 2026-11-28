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

/*
 *  The R5F loads the same spinlock firmware onto PRU0 and PRU1 of ICSSG0.
 *  The firmware drives no PRU pins, so no PRU pinmux or I/O expander setup
 *  is needed here.
 */

PRUICSS_Handle gPruIcss0Handle;

void spinlock_main(void *args)
{
    Drivers_open(); /* returns void, so there is no status to check */

    int status;
    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);

    gPruIcss0Handle = PRUICSS_open(CONFIG_PRU_ICSS0);
    DebugP_assert(gPruIcss0Handle != NULL);

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
