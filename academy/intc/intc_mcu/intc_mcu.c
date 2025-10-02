/*
 *  Copyright (C) 2024-2025 Texas Instruments Incorporated
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
#include <pru1_load_bin.h>
#if defined(SOC_AM64X) || defined(SOC_AM243X)
#include <rtupru0_load_bin.h>
#include <rtupru1_load_bin.h>
#include <txpru0_load_bin.h>
#include <txpru1_load_bin.h>
#endif

/*
 *  This is an example project to show R5F
 *  loading PRU firmware and generating system events via INTC module.
 */

/* Define system events, channels, and host interrupts */
#define PRU_SYSTEM_EVENT_16     16
#define PRU_SYSTEM_EVENT_60     60
#define PRU_HOST_INTR_2             2
#define PRUICSS_PRGM_FLOW_CNTRL_OFFSET  0xFC
#define MAX_INTERRUPT_COUNT     5

/**
 * @brief PRU Firmware Code Section
 */

#define INTR_RECEIVE_SECTION           0


PRUICSS_Handle gPruIcss0Handle;
SemaphoreP_Object gPruEventSem;
uint16_t numInterruptsReceived;

void PRU_InterruptHandler(void *args)
{
   /* Clear the event */
   PRUICSS_clearEvent(gPruIcss0Handle, PRU_SYSTEM_EVENT_16);
   
   /* Post semaphore to indicate interrupt received */
   SemaphoreP_post(&gPruEventSem);
   
   numInterruptsReceived += 1;
}

int32_t PRUICSS_goToSection(PRUICSS_Handle handle, uint32_t pruCore, uint32_t sectionId, uint32_t eventNum)
{
    uintptr_t               baseaddr;
    PRUICSS_HwAttrs const   *hwAttrs;
    int32_t                 retVal = SystemP_SUCCESS;

    if (handle != NULL)
    {
        hwAttrs  = (PRUICSS_HwAttrs const *)handle->hwAttrs;

        if (pruCore == PRUICSS_PRU0)
            baseaddr = hwAttrs->pru0DramBase;
        else if (pruCore == PRUICSS_PRU1)
            baseaddr = hwAttrs->pru1DramBase;
        /* RTU_PRU, TX_PRU not yet supported */
        else
            return SystemP_FAILURE;

        /* TODO: decide the final memory and offset */
        CSL_REG32_WR(baseaddr + PRUICSS_PRGM_FLOW_CNTRL_OFFSET, sectionId);
        retVal = PRUICSS_sendEvent(handle, eventNum);
    }
    else
    {
        retVal = SystemP_FAILURE;
    }
    return retVal;
}


void pru_io_intc_mcu_main(void *args)
{
   Drivers_open(); // check return status

   int status;
   status = Board_driversOpen();
   DebugP_assert(SystemP_SUCCESS == status);

   /* Create semaphore for synchronization */
   status = SemaphoreP_constructBinary(&gPruEventSem, 0);
   DebugP_assert(SystemP_SUCCESS == status);

   gPruIcss0Handle = PRUICSS_open(CONFIG_PRU_ICSS0);
   DebugP_assert(gPruIcss0Handle != NULL);

   /* Initialize INTC */
   #if defined(SOC_AM261X)
   status = PRUICSS_intcInit(gPruIcss0Handle, &icss1_intc_initdata);
   #else
   status = PRUICSS_intcInit(gPruIcss0Handle, &icss0_intc_initdata);
   #endif
   DebugP_assert(SystemP_SUCCESS == status);

   /* Register interrupt handler */
   status = PRUICSS_registerIrqHandler(gPruIcss0Handle, 
                                    PRU_HOST_INTR_2,      /* PRU event out number */
                                    0,              /* R5F Interrupt number */
                                    PRU_SYSTEM_EVENT_16, /* Event number */
                                    1,               /* Enable wait */
                                    PRU_InterruptHandler);
   DebugP_assert(SystemP_SUCCESS == status);

   status = PRUICSS_initMemory(gPruIcss0Handle, PRUICSS_DATARAM(PRUICSS_PRU0));
   DebugP_assert(status != 0);

   status = PRUICSS_initMemory(gPruIcss0Handle, PRUICSS_DATARAM(PRUICSS_PRU1));
   DebugP_assert(status != 0);

   numInterruptsReceived = 0;

   status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_PRU0, PRU0Firmware_0, sizeof(PRU0Firmware_0));
   DebugP_assert(SystemP_SUCCESS == status);
   status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_PRU1, PRU1Firmware_0, sizeof(PRU1Firmware_0));
   DebugP_assert(SystemP_SUCCESS == status);

   /* Main loop - wait for interrupts */
   while(1)
   {
      /* Wait for PRU interrupt */
      SemaphoreP_pend(&gPruEventSem, SystemP_WAIT_FOREVER);
      
      if (numInterruptsReceived >= MAX_INTERRUPT_COUNT) {
         PRUICSS_goToSection(gPruIcss0Handle, PRUICSS_PRU0, INTR_RECEIVE_SECTION, PRU_SYSTEM_EVENT_60);
         DebugP_log("Processed INTC generated system events. Maximum number of interrupts received: %d\r\n", numInterruptsReceived);
         DebugP_log("Sending interrupt to PRU Core 0.\r\n");
         break;
      }
   }

   #if defined(SOC_AM64X) || defined(SOC_AM243X)
   status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_RTU_PRU0, RTUPRU0Firmware_0, sizeof(RTUPRU0Firmware_0));
   DebugP_assert(SystemP_SUCCESS == status);
   status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_RTU_PRU1, RTUPRU1Firmware_0, sizeof(RTUPRU1Firmware_0));
   DebugP_assert(SystemP_SUCCESS == status);
   status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_TX_PRU0, TXPRU0Firmware_0, sizeof(TXPRU0Firmware_0));
   DebugP_assert(SystemP_SUCCESS == status);
   status = PRUICSS_loadFirmware(gPruIcss0Handle, PRUICSS_TX_PRU1, TXPRU1Firmware_0, sizeof(TXPRU1Firmware_0));
   DebugP_assert(SystemP_SUCCESS == status);
   #endif

   while (1)
   {
      ClockP_usleep(1);
   }

   Board_driversClose();
   Drivers_close();
}
