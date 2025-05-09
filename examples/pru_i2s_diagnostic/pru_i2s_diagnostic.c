/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
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
#include <kernel/dpl/SemaphoreP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "data.h"
#include "pru_i2s/include/pru_i2s_drv.h"
#include <stdint.h>
#include "ioexp_tca6416.h"
#define TEST_PRUI2S0_IDX    ( 0 )   /* Test PRU I2S 0 index */
#define TEST_PRUI2S1_IDX    ( 1 )   /* Test PRU I2S 1 index */

#define TDM4 ( 1 ) /* TDM4 mode  (change to 0 for I2S)*/ 
#ifdef TDM4
/* FW image data */
#include "pru_i2s/firmware/TDM4/pru_i2s_tdm4_pru0_array.h"  /* PRU0 */
#include "pru_i2s/firmware/TDM4/pru_i2s_tdm4_pru1_array.h"  /* PRU1 */
#else
/* FW image data */
#include "pru_i2s/firmware/I2S/pru_i2s_pru0_array.h"  /* PRU0 */
#include "pru_i2s/firmware/I2S/pru_i2s_pru1_array.h"  /* PRU1 */
#endif
/* PRU I2S PRU image info */
static PRUI2S_PruFwImageInfo gPruFwImageInfo[PRU_I2S_NUM_PRU_IMAGE] =
{
    {pru_prupru_i2s0_image_0_0, pru_prupru_i2s0_image_0_1, sizeof(pru_prupru_i2s0_image_0_0), sizeof(pru_prupru_i2s0_image_0_1)},
    {pru_prupru_i2s1_image_0_0, pru_prupru_i2s1_image_0_1, sizeof(pru_prupru_i2s1_image_0_0), sizeof(pru_prupru_i2s1_image_0_1)}
};
/* Configure I2C IO Expander */
void i2s_i2c_io_expander(void);
/* PRU I2S 0 Tx IRQ handler */
void pruI2s0TxIrqHandler(void *args);
/* PRU I2S 0 Error IRQ handler */
void pruI2s0ErrIrqHandler(void *args);

/* PRU I2S 1 Rx IRQ handler */
void pruI2s1RxIrqHandler(void *args);
/* PRU I2S 1 Error IRQ handler */
void pruI2s1ErrIrqHandler(void *args);

/* Rx buffers -- interleaved format */
#define RX_BUF_SZ       ( PRUI2S1_RX_PING_PONG_BUF_SZ/2*60 )    /* 60 PP buffers */
#define RX_BUF_SZ_32B   ( RX_BUF_SZ/4 )
__attribute__((section(".RxBuf"))) int32_t gPruI2s0RxBuf[RX_BUF_SZ_32B];
__attribute__((section(".RxBuf"))) int32_t gPruI2s1RxBuf[RX_BUF_SZ_32B];

/* Rx & Tx semaphores, used to synchronize PRU I2S ISRs with main task */
SemaphoreP_Object gPruI2s0TxSemObj;
SemaphoreP_Object gPruI2s1RxSemObj;

/* PRU I2S 0 transmit & receive control variables */
int32_t *gPPruI2s0TxBuf;            /* PRU I2S 0 transmit buffer pointer */
uint32_t gPruI2s0TxCnt;             /* PRU I2S 0 transmit count (all channels) */
uint16_t gPruI2s0XferSz;            /* PRU I2S 0 transfer size in 32-bit words */

/* PRU I2S 1 receive control variables */
int32_t *gPPruI2s1RxBuf;            /* PRU I2S 1 receive buffer pointer */
uint32_t gPruI2s1RxCnt;             /* PRU I2S 1 receive count (all channels) */
uint16_t gPruI2s1XferSz;            /* PRU I2S 1 transfer size in 32-bit words */

/* debug stats  */
/* PRU I2S 0 Tx ISR */
uint32_t gPruI2s0TxIsrCnt=0;        /* PRU I2S 0 Tx ISR (IRQ) count */
uint32_t gPruI2s0ClrTxIntErrCnt=0;  /* PRU I2S 0 clear Tx interrupt error count */
uint32_t gPruI2s0WrtErrCnt=0;       /* PRU I2S 0 write error count */
/* PRU I2S 0 error ISR */
uint32_t gPruI2s0ErrIsrCnt=0;       /* PRU I2S 0 error ISR (IRQ) count */
uint32_t gPruI2s0ClrErrIntErrCnt=0; /* PRU I2S 0 clear error interrupt error count */
uint32_t gPruI2s0ErrOvrCnt=0;       /* PRU I2S 0 Rx overflow error count */
uint32_t gPruI2s0ErrUndCnt=0;       /* PRU I2S 0 Tx underflow error count */
uint32_t gPruI2s0ErrFsyncCnt=0;     /* PRU I2S 0 frame sync error count */

/* PRU I2S 1 Rx ISR */
uint32_t gPruI2s1RxIsrCnt=0;        /* PRU I2S 1 Rx ISR (IRQ) count */
uint32_t gPruI2s1ClrRxIntErrCnt=0;  /* PRU I2S 1 clear Rx interrupt error count */
uint32_t gPruI2s1RdErrCnt=0;        /* PRU I2S 1 read error count */
/* PRU I2S 1 error ISR */
uint32_t gPruI2s1ErrIsrCnt=0;       /* PRU I2S 1 error ISR (IRQ) count */
uint32_t gPruI2s1ClrErrIntErrCnt=0; /* PRU I2S 1 clear error interrupt error count */
uint32_t gPruI2s1ErrOvrCnt=0;       /* PRU I2S 1 Rx overflow error count */
uint32_t gPruI2s1ErrUndCnt=0;       /* PRU I2S 1 Tx underflow error count */
uint32_t gPruI2s1ErrFsyncCnt=0;     /* PRU I2S 1 frame sync error count */

volatile Bool gRunFlag = TRUE;  /* Flag for continuing to execute test */

/* Define for PRU I2S Rx-to-Tx loopback */
//#define _DBG_PRUI2S_RX_TO_TX_LB
#ifdef _DBG_PRUI2S_RX_TO_TX_LB
uint8_t gPruI2s1NumRxI2s;
uint8_t gPruI2s0NumTxI2s;
float gPruI2s0LGain = 0.7071;       /* PRU I2S 0 Left channel gain, default -3 dB */
float gPruI2s0RGain = 0.5;          /* PRU I2S 0 Right channel gain, default -6 dB */
#endif

uint32_t gLoopCnt;

/*
 * This is an empty project provided for all cores present in the device.
 * User can use this project to start their application by adding more SysConfig modules.
 *
 * This application does driver and board init and just prints the pass string on the console.
 * In case of the main core, the print is redirected to the UART console.
 * For all other cores, CCS prints are used.
 */

static TCA6416_Config  gTCA6416_Config;

/* Configure I2C IO Expander 
    ICSSM1_MUX_SEL = 1,
    ICSSM2_MUX_SEL = 1
    
    => ICSSM Ethernet MUX U21, U17 route PR0_PRUn_GPOmm signals to HSEC
*/


void i2s_i2c_io_expander(void)
{
    int32_t             status = SystemP_SUCCESS;
    /* P20 = LED 3 bits, pin, 2 bits port.*/
    uint32_t            ioIndex = 0x10;
    TCA6416_Params      tca6416Params;


    TCA6416_Params_init(&tca6416Params);

    status = TCA6416_open(&gTCA6416_Config, &tca6416Params);

    if(status == SystemP_SUCCESS)
    {
        ioIndex = 0x02;

        /* Configure P02 as output  */
        status = TCA6416_config(
                      &gTCA6416_Config,
                      ioIndex,
                      TCA6416_MODE_OUTPUT);

        /* Set P02 to HIGH */
        status += TCA6416_setOutput(
                     &gTCA6416_Config,
                     ioIndex,
                     TCA6416_OUT_STATE_HIGH);


        /* Set P03 to HIGH */
        ioIndex = 0x03;
        status += TCA6416_setOutput(
                     &gTCA6416_Config,
                     ioIndex,
                     TCA6416_OUT_STATE_HIGH);

        /* Configure P03 as output  */
        status += TCA6416_config(
                      &gTCA6416_Config,
                      ioIndex,
                      TCA6416_MODE_OUTPUT);
    }
    
    TCA6416_close(&gTCA6416_Config);
}
void pru_i2s_diagnostic_main(void *args)
{
    uint8_t numValidCfg;
    PRUI2S_Params prms;
    PRUI2S_Handle hPruI2s0, hPruI2s1;
    PRUI2S_SwipAttrs swipAttrs;
    PRUI2S_IoBuf rdIoBuf;
    PRUI2S_IoBuf wrtIoBuf;
    int32_t status = PRUI2S_DRV_SOK;
#ifdef _DBG_PRUI2S_RX_TO_TX_LB
    int32_t *pSrc32b, *pDst32b;
    uint16_t i;
#endif

    /* Open drivers to open the UART driver for console */
    Drivers_open();
    Board_driversOpen();

    DebugP_log("PRU I2S test started ...\r\n");
    DebugP_log("Build timestamp      : %s %s\r\n", __DATE__, __TIME__);

    /* Debug, configure GPIO */
    GPIO_setDirMode(CONFIG_GPIO_DEBUG0_BASE_ADDR, CONFIG_GPIO_DEBUG0_PIN, CONFIG_GPIO_DEBUG0_DIR);
    GPIO_pinWriteHigh(CONFIG_GPIO_DEBUG0_BASE_ADDR, CONFIG_GPIO_DEBUG0_PIN);
    GPIO_pinWriteLow(CONFIG_GPIO_DEBUG0_BASE_ADDR, CONFIG_GPIO_DEBUG0_PIN);
    GPIO_setDirMode(CONFIG_GPIO_DEBUG1_BASE_ADDR, CONFIG_GPIO_DEBUG1_PIN, CONFIG_GPIO_DEBUG1_DIR);
    GPIO_pinWriteHigh(CONFIG_GPIO_DEBUG1_BASE_ADDR, CONFIG_GPIO_DEBUG1_PIN);
    GPIO_pinWriteLow(CONFIG_GPIO_DEBUG1_BASE_ADDR, CONFIG_GPIO_DEBUG1_PIN);

    /* Configure I2C IO Expander,
       route PRUn signals to HSEC */
    i2s_i2c_io_expander();

    /* Construct semaphores */
    status = SemaphoreP_constructBinary(&gPruI2s0TxSemObj, 0);
    if (status == SystemP_FAILURE)
    {
        DebugP_log("ERROR: SemaphoreP_constructBinary() gPruI2s0TxSemObj\r\n");
        return;
    }
    status = SemaphoreP_constructBinary(&gPruI2s1RxSemObj, 0);
    if (status == SystemP_FAILURE)
    {
        DebugP_log("ERROR: SemaphoreP_constructBinary() gPruI2s1RxSemObj\r\n");
        return;
    }

    /* Initialize PRU I2S driver */
    status = PRUI2S_init(&numValidCfg, &gPruFwImageInfo);
    if (status == PRUI2S_DRV_SERR_INIT_FWIMG)
    {
        DebugP_log("WARNING: PRUI2S_init() no FW image for configuration\r\n");
    }
    else if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_init()\r\n");
        return;
    }
    DebugP_assert(numValidCfg != 0);

    if (((numValidCfg-1) < TEST_PRUI2S0_IDX) || 
        ((numValidCfg-1) < TEST_PRUI2S1_IDX))
    {
        DebugP_log("ERROR: Invalid test configuration()\r\n");
        return;
    }

    /*
     *  Open and configure PRU I2S 0
     */

    /* Get SWIP attributes for PRU I2S 0 instance */
    status = PRUI2S_getInitCfg(TEST_PRUI2S0_IDX, &swipAttrs);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_getInitCfg() PRU I2S 0\r\n");
        return;
    }
    /* Check PRU I2S0 instance is Tx only */
    DebugP_assert((swipAttrs.numTxI2s > 0) && (swipAttrs.numRxI2s == 0));

    /* Set PRU I2S 0 transfer size.
        \2 for ping or pong. */
    gPruI2s0XferSz = PRUI2S0_TX_PING_PONG_BUF_SZ_32B/2;
    
    /* Open PRU I2S 0 instance */
    PRUI2S_paramsInit(&prms);
    prms.rxPingPongBaseAddr = 0;
    prms.txPingPongBaseAddr = (uint32_t)PRUI2S0_TX_PING_PONG_BASE_ADDR;
    prms.pingPongBufSz = PRUI2S0_TX_PING_PONG_BUF_SZ;
    prms.i2sTxCallbackFxn = &pruI2s0TxIrqHandler;
    prms.i2sRxCallbackFxn = NULL;
    prms.i2sErrCallbackFxn = &pruI2s0ErrIrqHandler;
    hPruI2s0 = PRUI2S_open(TEST_PRUI2S0_IDX, &prms);
    if (hPruI2s0 == NULL)
    {
        DebugP_log("ERROR: PRUI2S_open() PRU I2S 0\r\n");
        return;
    }

    /* Initialize PRU I2S 0 transmit pointer and count */
    gPPruI2s0TxBuf = &gPruI2s0TxBuf[0];
    gPruI2s0TxCnt = 0;

#ifdef _DBG_PRUI2S_RX_TO_TX_LB
    /* set PRU I2S0 number of Tx I2S */
    gPruI2s0NumTxI2s = swipAttrs.numTxI2s;
#endif

    /*
     *  Open and configure PRU I2S 1
     */

    /* Get SWIP attributes for PRU I2S 1 instance */
    status = PRUI2S_getInitCfg(TEST_PRUI2S1_IDX, &swipAttrs);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_getInitCfg() PRU I2S 1\r\n");
        return;
    }
    /* Check PRU I2S1 instance is Rx only */
    DebugP_assert((swipAttrs.numTxI2s == 0) && (swipAttrs.numRxI2s > 0));

    /* Set PRU I2S 1 transfer size.
        \2 for ping or pong. */
    gPruI2s1XferSz = PRUI2S1_RX_PING_PONG_BUF_SZ_32B/2;

    /* Open PRU I2S 1 instance */
    PRUI2S_paramsInit(&prms);
    prms.rxPingPongBaseAddr = (uint32_t)gPruI2s1RxPingPongBuf;
    prms.txPingPongBaseAddr = 0;
    prms.pingPongBufSz = PRUI2S1_RX_PING_PONG_BUF_SZ;
    prms.i2sTxCallbackFxn = NULL;
    prms.i2sRxCallbackFxn = &pruI2s1RxIrqHandler;
    prms.i2sErrCallbackFxn = &pruI2s1ErrIrqHandler;
    hPruI2s1 = PRUI2S_open(TEST_PRUI2S1_IDX, &prms);
    if (hPruI2s1 == NULL)
    {
        DebugP_log("ERROR: PRUI2S_open() PRU I2S 1\r\n");
        return;
    }

    /* Initialize PRU I2S 1 receive pointer and count */
    gPPruI2s1RxBuf=&gPruI2s1RxBuf[0];
    gPruI2s1RxCnt=0;
    
#ifdef _DBG_PRUI2S_RX_TO_TX_LB
    /* set PRU I2S1 number of Rx I2S */
    gPruI2s1NumRxI2s = swipAttrs.numRxI2s;
#endif   

    /*
     *  Enable PRU I2S 0 & 1
     */

    /* Enable PRU I2S 0 */
    status = PRUI2S_enable(hPruI2s0);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_enable() PRU I2S 0\r\n");
        return;
    }

    /* Enable PRU I2S 1 */
    status = PRUI2S_enable(hPruI2s1);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_enable() PRU I2S 1\r\n");
        return;
    }

    gLoopCnt = 0;
    while (gRunFlag == TRUE)
    {
        /* Write next PRU I2S0 Tx ping/pong buffer */
       PRUI2S_ioBufInit(&wrtIoBuf);
       wrtIoBuf.ioBufAddr = gPPruI2s0TxBuf;
       status = PRUI2S_write(hPruI2s0, &wrtIoBuf);
       if (status != PRUI2S_DRV_SOK)
       {
           gPruI2s0WrtErrCnt++;
       }
        /* Wait for all PRU I2S events */
        SemaphoreP_pend(&gPruI2s1RxSemObj, SystemP_WAIT_FOREVER);
        SemaphoreP_pend(&gPruI2s0TxSemObj, SystemP_WAIT_FOREVER);
        gLoopCnt++;
        /* Read next PRU I2S1 Rx ping/pong buffer */
        PRUI2S_ioBufInit(&rdIoBuf);
        rdIoBuf.ioBufAddr = gPPruI2s1RxBuf;
        status = PRUI2S_read(hPruI2s1, &rdIoBuf);
        if (status != PRUI2S_DRV_SOK)
        {
            gPruI2s1RdErrCnt++;
        }
#ifdef _DBG_PRUI2S_RX_TO_TX_LB
        pSrc32b = gPPruI2s1RxBuf;
        pDst32b = gPPruI2s0TxBuf;
        for (i = 0; i < gPruI2s0XferSz/(2*gPruI2s0NumTxI2s); i++)
        {
            /* Compute Tx left-ch outputs:
                Ltx0 = Lrx0 * Gl
                Ltx1 = Lrx1 * Gl
                Ltx2 = (Lrx0 + Lrx1)/2
            */
            pDst32b[0] = (int32_t)(pSrc32b[0] * gPruI2s0LGain);
            pDst32b[1] = (int32_t)(pSrc32b[1] * gPruI2s0LGain);
            pDst32b[2] = (int32_t)((pSrc32b[0] + pSrc32b[1])/2);
            pSrc32b += gPruI2s1NumRxI2s;
            pDst32b += gPruI2s0NumTxI2s;
            
            /* Compute Tx right-ch outputs:
                Rtx0 = Rrx0 * Gr
                Rtx1 = Rrx1 * Gr
                Rtx2 = (Rrx0 + Rrx1)/2
            */
            pDst32b[0] = (int32_t)(pSrc32b[0] * gPruI2s0RGain);
            pDst32b[1] = (int32_t)(pSrc32b[1] * gPruI2s0RGain);
            pDst32b[2] = (int32_t)((pSrc32b[0] + pSrc32b[1])/2);
            pSrc32b += gPruI2s1NumRxI2s;
            pDst32b += gPruI2s0NumTxI2s;
        }
#endif
        

        /* Update PRU I2S1 Rx buffer pointer & count */
        gPruI2s1RxCnt += gPruI2s1XferSz;
        gPPruI2s1RxBuf += gPruI2s1XferSz;
        if (gPruI2s1RxCnt >= RX_BUF_SZ_32B)
        {
            gPPruI2s1RxBuf = &gPruI2s1RxBuf[0];
            gPruI2s1RxCnt = 0;
        }
        /* Update PRU I2S0 Tx buffer pointer & count */
        gPruI2s0TxCnt += gPruI2s0XferSz;
        gPPruI2s0TxBuf += gPruI2s0XferSz;
        if (gPruI2s0TxCnt >= TX_BUF_SZ_32B)
        {
            gPPruI2s0TxBuf = &gPruI2s0TxBuf[0];
            gPruI2s0TxCnt = 0;
        }
    }

    /* Close PRU I2S 0 instance */
    status = PRUI2S_close(hPruI2s0);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_close() PRU I2S 0\r\n");
        return;
    }

    /* Close PRU I2S 1 instance */
    status = PRUI2S_close(hPruI2s1);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_close() PRU I2S 1\r\n");
        return;
    }
    
    /* De-initialize PRU I2S driver */
    PRUI2S_deinit();
    
    /* Destroy semaphores */
    SemaphoreP_destruct(&gPruI2s0TxSemObj);
    SemaphoreP_destruct(&gPruI2s1RxSemObj);

    DebugP_log("All tests have passed!!\r\n");

    Board_driversClose();
    Drivers_close();
}

/* PRU I2S 0 Tx IRQ handler -- write/read */
void pruI2s0TxIrqHandler(void *args)
{
    PRUI2S_Handle handle;
    int32_t status;

    /* debug, increment ISR count */
    gPruI2s0TxIsrCnt++;
    /* debug, drive GPIO high */
    GPIO_pinWriteHigh(CONFIG_GPIO_DEBUG0_BASE_ADDR, CONFIG_GPIO_DEBUG0_PIN);
    
    handle = (PRUI2S_Handle)args;
    
    /* Clear PRU I2S interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_TX);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s0ClrTxIntErrCnt++;
    }

    SemaphoreP_post(&gPruI2s0TxSemObj);

    /* debug, drive GPIO low */
    GPIO_pinWriteLow(CONFIG_GPIO_DEBUG0_BASE_ADDR, CONFIG_GPIO_DEBUG0_PIN);
}

/* PRU I2S 0 Error IRQ handler */
void pruI2s0ErrIrqHandler(void *args)
{
    PRUI2S_Handle handle;
    uint8_t errStat;
    int32_t status;

    /* debug, increment ISR count */
    gPruI2s0ErrIsrCnt++;

    handle = (PRUI2S_Handle)args;
       
    /* Clear PRU I2S interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_ERR);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s0ClrErrIntErrCnt++;
    }
    
    /* Get error status */
    PRUI2S_getErrStat(handle, &errStat);
    
    if (errStat & (1 << I2S_ERR_OVR_BN))
    {
        /* Increment Rx overflow error count */
        gPruI2s0ErrOvrCnt++;
    }
    if (errStat & (1 << I2S_ERR_UND_BN))
    {
        /* Increment Tx underflow error count */
        gPruI2s0ErrUndCnt++;
    }
    if (errStat & (1 << I2S_ERR_FSYNC_BN))
    {
        /* Increment frame sync error count */
        gPruI2s0ErrFsyncCnt++;
    }

    /* Clear error status */
    PRUI2S_clearErrStat(handle, errStat);
}

/* PRU I2S 1 Rx IRQ handler */
void pruI2s1RxIrqHandler(void *args)
{
    PRUI2S_Handle handle;
    int32_t status;

    /* debug, increment ISR count */
    gPruI2s1RxIsrCnt++;
    /* debug, drive GPIO high */
    GPIO_pinWriteHigh(CONFIG_GPIO_DEBUG1_BASE_ADDR, CONFIG_GPIO_DEBUG1_PIN);

    handle = (PRUI2S_Handle)args;
    
    /* Clear PRU I2S interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_RX);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s1ClrRxIntErrCnt++;
    }

    SemaphoreP_post(&gPruI2s1RxSemObj);
    
    /* debug, drive GPIO low */
    GPIO_pinWriteLow(CONFIG_GPIO_DEBUG1_BASE_ADDR, CONFIG_GPIO_DEBUG1_PIN);
}

/* PRU I2S 1 Error IRQ handler */
void pruI2s1ErrIrqHandler(void *args)
{
    PRUI2S_Handle handle;
    uint8_t errStat;
    int32_t status;

    /* debug, increment ISR count */
    gPruI2s1ErrIsrCnt++;

    handle = (PRUI2S_Handle)args;
    
    /* Clear PRU I2S interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_ERR);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s1ClrErrIntErrCnt++;
    }

    /* Get error status */
    PRUI2S_getErrStat(handle, &errStat);
    
    if (errStat & (1 << I2S_ERR_OVR_BN))
    {
        /* Increment Rx overflow error count */
        gPruI2s1ErrOvrCnt++;
    }
    if (errStat & (1 << I2S_ERR_UND_BN))
    {
        /* Increment Tx underflow error count */
        gPruI2s1ErrUndCnt++;
    }
    if (errStat & (1 << I2S_ERR_FSYNC_BN))
    {
        /* Increment frame sync error count */
        gPruI2s1ErrFsyncCnt++;
    }

    /* Clear error status */
    PRUI2S_clearErrStat(handle, errStat);    
}
