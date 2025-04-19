/*
 *  Copyright (C) 2025 Texas Instruments Incorporated
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
#include "pruemif16.h"

/* ICSSG instance ID */
#define APP_ICSSG_INST_ID               ( CONFIG_PRU_ICSS0 )

/* Status codes */
#define PRUEMIF16_ERR_NERR              (  0 )  /* no error */
#define PRUEMIF16_ERR_INIT_ICSSG        ( -1 )  /* initialize ICSSG error */
#define PRUEMIF16_ERR_INIT_PRU_SDDF     ( -2 )  /* initialize PRU error */

/* PRUICSS driver handle */
PRUICSS_Handle gPruIcssHandle;

#define _DBG_INIT_SHMEM_BUFFER
#ifdef _DBG_INIT_SHMEM_BUFFER
#define SHMEM_BUFFER_BASE_ADDR  ( 0x30010000 )
#define SHMEM_BUFFER_SZ         ( 256 )     /* length in 16-bit words */

/* Data mask for unconnected data bus lines */
/* Missing bits(D10,D9,D8) are masked with 0 */
#define DATA_MASK               (~(1<<10 | 1<<9 | 1<<7) & 0x1FFF) 

#endif

void pruemif16_main(void *args)
{
    int32_t status;

    /* Open drivers to open the UART driver for console */
    Drivers_open();
    Board_driversOpen();

    /* Initialize ICSSG */
    status = pruEmif16_InitIcss(APP_ICSSG_INST_ID, &gPruIcssHandle);
    if (status != PRUEMIF16_ERR_NERR) 
    {
        DebugP_log("Error: pruEmif16_InitIcss() fail.\r\n");
        return;
    }

    /* Initialize PRU0 core */
    status = pruEmif16_initPruIcss(gPruIcssHandle, APP_ICSSG_INST_ID, PRUICSS_PRU0);
    if (status != PRUEMIF16_ERR_NERR) 
    {
        DebugP_log("Error: pruEmif16_initPruIcss() fail.\r\n");
        return;
    }

    /* Initialize PRU1 core */
    status = pruEmif16_initPruIcss(gPruIcssHandle, APP_ICSSG_INST_ID, PRUICSS_PRU1);
    if (status != PRUEMIF16_ERR_NERR) 
    {
        DebugP_log("Error: pruEmif16_initPruIcss() fail.\r\n");
        return;
    }

    /* Enable PRU0 */
    status = PRUICSS_enableCore(gPruIcssHandle, PRUICSS_PRU0);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log("Error: PRUICSS_enableCore() fail.\r\n");
        return;
    }

    /* Enable PRU1 */
    status = PRUICSS_enableCore(gPruIcssHandle, PRUICSS_PRU1);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log("Error: PRUICSS_enableCore() fail.\r\n");
        return;
    }

#ifdef _DBG_INIT_SHMEM_BUFFER
    {
        uint32_t i;
        uint16_t *ptr;
        uint16_t memVal;
        
        ptr = (uint16_t *)SHMEM_BUFFER_BASE_ADDR;
        memVal = 0xFFFF;
        for (i = 0; i < SHMEM_BUFFER_SZ; i++)
        {
            *ptr++ = memVal & DATA_MASK;
            memVal -= 0x0101;
        }
    }
#endif

    while(1);
    

    DebugP_log("All tests have passed!!\r\n");

    Board_driversClose();
    Drivers_close();
}
