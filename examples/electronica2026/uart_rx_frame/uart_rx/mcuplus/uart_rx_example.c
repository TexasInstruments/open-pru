/*
 *  Copyright (C) 2024-2026 Texas Instruments Incorporated
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

#include <pru0_load_bin.h>

/*
 *  R5F host for the UART RX frame example.
 *
 *  Loads and starts only ICSSG0 PRU0. The PRU0 firmware bit-bangs a
 *  4 Mbaud 8N1 UART receiver on PRG0_PRU0_GPI2 (R31 bit 2) and stores each
 *  11-byte frame in PRU0 DRAM0 (ring buffer at offsets 0x0000-0x0FFB,
 *  framing error flag at 0x0FFE). Pin mux, input pull-up and the 200 MHz
 *  PRU core clock come from example.syscfg.
 *  No other PRU, RTU_PRU or TX_PRU core of ICSSG0 is loaded.
 */

/** \brief Handle to the ICSSG0 instance (CONFIG_PRU_ICSS0 in SysConfig) */
PRUICSS_Handle gPruIcss0Handle;

void uart_rx_example_main(void *args)
{
    int32_t status;

    Drivers_open();

    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);

    gPruIcss0Handle = PRUICSS_open(CONFIG_PRU_ICSS0);
    DebugP_assert(gPruIcss0Handle != NULL);

    /* Clear PRU0 DRAM: frame ring buffer and framing error flag (0x0FFE) */
    status = PRUICSS_initMemory(gPruIcss0Handle, PRUICSS_DATARAM(PRUICSS_PRU0));
    DebugP_assert(status != 0);

    /* Firmware uses R30/R31 in direct GPIO mode (GP_MUX_SEL = GP) */
    status = PRUICSS_setGpMuxSelect(gPruIcss0Handle, PRUICSS_PRU0,
                                    PRUICSS_GP_MUX_SEL_MODE_GP);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Load PRU0 IRAM, reset and run the core */
    status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_PRU0,
                                  PRU0Firmware_0, sizeof(PRU0Firmware_0));
    DebugP_assert(SystemP_SUCCESS == status);

    DebugP_log("UART RX frame: ICSSG0 PRU0 running, frames at PRU0 DRAM0 (0x30000000)\r\n");

    while (1)
    {
        ClockP_usleep(1);
    }

    Board_driversClose();
    Drivers_close();
}
