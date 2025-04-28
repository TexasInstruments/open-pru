/*
 * Copyright (C) 2021 Texas Instruments Incorporated
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 *   Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
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

#include <stdint.h>
#include "pru_i2s_drv.h"
#include <pru_i2s/firmware/TDM4/icss_pru_i2s_fw.h>
/* 
 *   Initializes PRU I2S driver.
 *   
 *   Must be called before any other API functions are called.
 *
 *   Reads information from PRU0/1 FW concerning supported I2S features &
 *   updates driver Configuration table (SW IP entries). Information is read 
 *   from FW pseudo registers contained in FW DMEM images.
 *
 *   The following information is updated in each SW IP entry:
 *       - # Tx I2S
 *       - # Rx I2S
 *       - # Tx/Rx I2S
 *       - Sampling frequency
 *       - Slot width (# bits)
 *       - Tx ping/pong buffer base address
 *       - Tx/Rx buffer (ping+pong) sizes
 *       - System Event Numbers
 *       - Pins (Bclk,Fsync,Tx,Rx).
 *
 *  Parameters: 
 *      pNumValidCfg: Pointer to number of valid configurations
 *
 *  return: status code 
 */
int32_t PRUI2S_init(
    uint8_t *pNumValidCfg, PRUI2S_PruFwImageInfo (*gPruFwImageInfo)[2]
)
{
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    int32_t status;
    int32_t retStatus = PRUI2S_DRV_SOK;
    uint8_t i;
       
    if (gPruI2sDrvInit == FALSE) 
    {
        /* 
           Initialize PRU I2S configurations in configuration table.
           Stop processing configurations if error occurs while
           initializing particular configuration.
         */
        for (i = 0; i < PRU_I2S_NUM_CONFIG; i++)
        {
            /* Get pointers */
            pObj = gPruI2sConfig[i].object;
            pSwipAttrs = gPruI2sConfig[i].attrs;

            /* 
             * Initialize Object
             */

            memset(pObj, 0, sizeof(PRUI2S_Object));
            pObj->isOpen = FALSE;
            pObj->pruIcssHandle = NULL;

            /* Construct PRU instance lock */
            status = SemaphoreP_constructMutex(&pObj->pruInstlockObj);
            if (status == SystemP_SUCCESS)
            {
                pObj->pruInstLock = &pObj->pruInstlockObj;
            }
            else
            {
                retStatus = PRUI2S_DRV_SERR_INIT;
                break;
            }
            
            /* 
             * Initialize SW IP using information in PRU images.
             *
             *  Currently up to two PRU images supported and images are mapped 
             *  to PRU cores as follows:
             *      - PRU image #0 -> ICSSn/PRU0
             *      - PRU image #1 -> ICSSn/PRU1
             *  The same PRU image can be mapped to PRU0/1 by having PRU
             *  image pointers in gPruImageAddr[] point to the same image.
             */
            if ((pSwipAttrs->pruInstId == PRUICSS_PRU0) && ((*gPruFwImageInfo)[0].pPruImemImg != NULL))
            {
                /* Update base address */
                pSwipAttrs->baseAddr = (uint32_t)AddrTranslateP_getLocalAddr(pSwipAttrs->baseAddr);

                /* Parse info in PRU image #0, update SW IP info */
                status = PRUI2S_getPruFwImageInfo(&((*gPruFwImageInfo)[0]), pSwipAttrs);
                if (status != SystemP_SUCCESS)
                {
                    retStatus = PRUI2S_DRV_SERR_INIT;
                    break;
                }
                
                /* Set FW image pointer */
                pObj->pPruFwImageInfo = &((*gPruFwImageInfo)[0]);
            }
            else if ((pSwipAttrs->pruInstId == PRUICSS_PRU1) && ((*gPruFwImageInfo)[1].pPruImemImg != NULL))
            {
                /* Update base address */
                pSwipAttrs->baseAddr = (uint32_t)AddrTranslateP_getLocalAddr(pSwipAttrs->baseAddr);

                /* Parse info in PRU image #1, update SW IP info */
                status = PRUI2S_getPruFwImageInfo(&((*gPruFwImageInfo)[1]), pSwipAttrs);
                if (status != SystemP_SUCCESS)
                {
                    retStatus = PRUI2S_DRV_SERR_INIT;
                    break;
                }
                
                /* Set FW image pointer */
                pObj->pPruFwImageInfo = &((*gPruFwImageInfo)[1]);
            }
            else 
            {
                /* Error, no PRU FW image for selected core type */
                retStatus = PRUI2S_DRV_SERR_INIT_FWIMG;
                break;
            }
            
            /* Check SW IP parameters */
            status = PRUI2S_checkSwipParams(pSwipAttrs);
            if (status != SystemP_SUCCESS)
            {
                retStatus = PRUI2S_DRV_SERR_INIT;
                break;
            }
            
            gPruI2sDrvNumValidCfg++;
        }
        
        *pNumValidCfg = gPruI2sDrvNumValidCfg;
        if (retStatus == PRUI2S_DRV_SOK)
        {
            gPruI2sDrvInit = TRUE;
        }
        
    }
    
    return retStatus;
}
/* 
 *  De-initializes PRU I2S driver. 
 *
 *  Parameters: none
 *
 *  return: none
 */
void PRUI2S_deinit(void)
{
    PRUI2S_Object *pObj;
    uint8_t i;

    if (gPruI2sDrvInit == TRUE)
    {
        for (i = 0; i < gPruI2sDrvNumValidCfg; i++)
        {
            /* Get object pointer */
            pObj = gPruI2sConfig[i].object;

            /* Destroy PRU instance lock */                
            SemaphoreP_destruct(pObj->pruInstLock);
        }
    }
}

/* 
 *   Get PRUI2S SW IP attributes for use in application, e.g.
 *      - # Tx I2S
 *      - # Rx I2S
 *      - Sampling frequency
 *      - Slot width (#bits)
 *      - Tx/Rx buffer (ping+pong) size.
 * 
 *  Parameters:
 *      index       Index of config to use in the Config array
 *      pCfg        Pointer to SW IP in selected config
 *
 *  return: status code
 */
int32_t PRUI2S_getInitCfg(
    uint32_t index, 
    PRUI2S_SwipAttrs *pCfg
)
{
    int32_t retStatus = PRUI2S_DRV_SOK;
    
    if (index < gPruI2sDrvNumValidCfg)
    {
        *pCfg = gPruI2sSwipAttrs[index];
    }
    else
    {
        retStatus = PRUI2S_DRV_SERR_INV_PRMS;
    }
    
    return retStatus;
}

/* 
 *  Sets PRU I2S default parameters. 
 *
 *  Parameters:
 *      pPrms       Pointer to PRUI2S_Params structure to initialize
 *
 *  return: none
 */
void PRUI2S_paramsInit(
    PRUI2S_Params *pPrms
)
{
    pPrms->pruI2sEn = FALSE;
    
    pPrms->rxPingPongBaseAddr = 0;    
    
    pPrms->i2sTxIntrPri = DEF_I2S_INTR_PRI;
    pPrms->i2sTxCallbackFxn = NULL;
    
    pPrms->i2sRxIntrPri = DEF_I2S_INTR_PRI;
    pPrms->i2sRxCallbackFxn = NULL;

    pPrms->i2sErrIntrPri = DEF_I2S_ERR_INTR_PRI;
    pPrms->i2sErrCallbackFxn = NULL;
}

/* 
 *  Opens PRU I2S instance:
 *      - Clears associated ICSS IMEM/DMEM
 *      - Initializes ICSS INTC
 *      - Downloads firmware to PRU (IMEM/DMEM)
 *      - Constructs interrupts & registers interrupt callbacks.
 *
 *  Parameters:
 *      index       Index of config to use in the Config array
 *      pPrms       Pointer to open parameters. If NULL is passed, then
 *                      default values will be used
 *
 *  return: A PRUI2S_Handle on success or a NULL on an error occurred
 */
PRUI2S_Handle PRUI2S_open(
    uint32_t index, 
    PRUI2S_Params *pPrms
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    PRUI2S_Handle handle = NULL;
    HwiP_Params hwiPrms;
    int32_t status = SystemP_SUCCESS;
    
    if (index < gPruI2sDrvNumValidCfg)
    {
        /* Get pointers */
        pCfg = &gPruI2sConfig[index];
        pObj = pCfg->object;
        pSwipAttrs = pCfg->attrs;
    }
    else
    {
        return handle;
    }

    /* Lock instance */
    SemaphoreP_pend(pObj->pruInstLock, SystemP_WAIT_FOREVER);
    
    if ((pObj->isOpen == FALSE) && (pObj->pruIcssHandle == NULL))
    {
        /* Copy parameters to Object */
        if (pPrms != NULL)
        {
            pObj->prms = *pPrms;
        }
        else
        {
            PRUI2S_paramsInit(&pObj->prms);
        }
        
        /* Check parameters are valid */
        status = PRUI2S_checkOpenParams(pSwipAttrs, &pObj->prms);
    }
    
    if (status == SystemP_SUCCESS)
    {
        /* 
         * Construct interrupts
         */
         
        if (pPrms->i2sTxCallbackFxn != NULL)
        {
            /* Construct I2S Tx interrupt */
            HwiP_Params_init(&hwiPrms);
            hwiPrms.intNum = pSwipAttrs->i2sTxHostIntNum;
            hwiPrms.callback = pObj->prms.i2sTxCallbackFxn;
            hwiPrms.priority = pObj->prms.i2sTxIntrPri;
            hwiPrms.args = (void *)pCfg;
            status = HwiP_construct(&pObj->i2sTxHwiObj, &hwiPrms);
            if (status == SystemP_SUCCESS)
            {
                pObj->i2sTxHwiHandle = &pObj->i2sTxHwiObj;
            }
        }

        if ((status == SystemP_SUCCESS) && (pPrms->i2sRxCallbackFxn != NULL))
        {
            /* Construct I2S Rx interrupt */
            HwiP_Params_init(&hwiPrms);
            hwiPrms.intNum = pSwipAttrs->i2sRxHostIntNum;
            hwiPrms.callback = pObj->prms.i2sRxCallbackFxn;
            hwiPrms.priority = pObj->prms.i2sRxIntrPri;
            hwiPrms.args = (void *)pCfg;
            status = HwiP_construct(&pObj->i2sRxHwiObj, &hwiPrms);
            if (status == SystemP_SUCCESS)
            {
                pObj->i2sRxHwiHandle = &pObj->i2sRxHwiObj;
            }
        }

        if ((status == SystemP_SUCCESS) && (pPrms->i2sErrCallbackFxn != NULL))
        {
            /* Construct I2S error interrupt */
            HwiP_Params_init(&hwiPrms);
            hwiPrms.intNum = pSwipAttrs->i2sErrHostIntNum;
            hwiPrms.callback = pObj->prms.i2sErrCallbackFxn;
            hwiPrms.priority = pObj->prms.i2sErrIntrPri;
            hwiPrms.args = (void *)pCfg;
            status = HwiP_construct(&pObj->i2sErrHwiObj, &hwiPrms);
            if (status == SystemP_SUCCESS)
            {
                pObj->i2sErrHwiHandle = &pObj->i2sErrHwiObj;
            }
        }
    }

    if (status == SystemP_SUCCESS)
    {
        /* 
         * Initialize PRU 
         */

        status = PRUI2S_initPru(pCfg);
        
        /* Initialize ICSS INTC */
        if (status == SystemP_SUCCESS)
        {
            status = PRUI2S_initIcssIntc(pCfg);
        }

        /* Initialize PRU I2S FW */
        if (status == SystemP_SUCCESS)
        {
            status = PRUI2S_initFw(pCfg);
        }
        
        /* Initialize ping/pong buffers */
        if (status == SystemP_SUCCESS)
        {
            status = PRUI2S_initPpBufs(pCfg);
        }
        
        /* Configure SoC PADs */
        if (status == SystemP_SUCCESS)
        {
            PRUI2S_initFwPadRegs(pCfg);
            status = PRUI2S_initPruGpioOutCtrlReg(pCfg);
        }
    }
    
    /* Enable PRU */
    if ((status == SystemP_SUCCESS) && (pObj->prms.pruI2sEn == TRUE))
    {
        /* Enable PRU core */
        status = PRUICSS_enableCore(pObj->pruIcssHandle, pSwipAttrs->pruInstId);
    }
           
    if (status == SystemP_SUCCESS)
    {
        /* Mark Object as open */
        pObj->isOpen = TRUE;
        /* Set return handle */
        handle = (PRUI2S_Handle)pCfg;
    }
    
    /* Unlock instance */
    SemaphoreP_post(pObj->pruInstLock);
    
    if (status != SystemP_SUCCESS)
    {
        PRUI2S_close((PRUI2S_Handle)pCfg);
    }
    
    return handle;
}

/* Closes PRU I2S instance:
 *  - Disables PRU
 *  - Resets PRU
 *  - Clears associated ICSS IMEM/DMEM.
 *  - Destroys interrupts.
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *
 *  return: status code
*/
int32_t PRUI2S_close(
    PRUI2S_Handle handle
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    int32_t status = SystemP_SUCCESS;
    int32_t retStatus = PRUI2S_DRV_SOK;
    
    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    /* Lock instance */
    SemaphoreP_pend(pObj->pruInstLock, SystemP_WAIT_FOREVER);

    /* 
     * Destroy interrupts
     */
    if (pObj->i2sTxHwiHandle != NULL)
    {
        /* Disable I2S Tx interrupt */
        status = PRUICSS_disableEvent(pObj->pruIcssHandle, pSwipAttrs->i2sTxIcssIntcSysEvt);
        
        if (status == SystemP_SUCCESS)
        {
            /* Clear I2S Tx pending interrupts */
            status = PRUICSS_clearEvent(pObj->pruIcssHandle, pSwipAttrs->i2sTxIcssIntcSysEvt);
        }

        if (status == SystemP_SUCCESS)
        {
            /* Destroy I2S Tx interrupt */
            HwiP_destruct(&pObj->i2sTxHwiObj);
            pObj->i2sTxHwiHandle = NULL;
        }
    }

    if ((status == SystemP_SUCCESS) && (pObj->i2sRxHwiHandle != NULL))
    {
        /* Disable I2S Rx interrupt */
        status = PRUICSS_disableEvent(pObj->pruIcssHandle, pSwipAttrs->i2sRxIcssIntcSysEvt);
        
        if (status == SystemP_SUCCESS)
        {
            /* Clear I2S Rx pending interrupts */
            status = PRUICSS_clearEvent(pObj->pruIcssHandle, pSwipAttrs->i2sRxIcssIntcSysEvt);
        }

        if (status == SystemP_SUCCESS)
        {
            /* Destroy I2S Rx interrupt */
            HwiP_destruct(&pObj->i2sRxHwiObj);
            pObj->i2sRxHwiHandle = NULL;
        }
    }

    if ((status == SystemP_SUCCESS) && (pObj->i2sErrHwiHandle != NULL))
    {
        /* Disable I2S error interrupt */
        status = PRUICSS_disableEvent(pObj->pruIcssHandle, pSwipAttrs->i2sErrIcssIntcSysEvt);
        
        /* Clear I2S error pending interrupts */
        if (status == SystemP_SUCCESS)
        {
            /* Clear I2S pending interrupts */
            status = PRUICSS_clearEvent(pObj->pruIcssHandle, pSwipAttrs->i2sErrIcssIntcSysEvt);
        }

        if (status == SystemP_SUCCESS)
        {
            /* Destroy I2S error interrupt */
            HwiP_destruct(&pObj->i2sErrHwiObj);
            pObj->i2sErrHwiHandle = NULL;
        }
    }
        
    if (status == SystemP_SUCCESS)
    {
        /* Deinitialize PRU */
        status = PRUI2S_deinitPru(pCfg);
    }

    if (status == SystemP_SUCCESS)
    {
        /* Mark Object as closed */
        pObj->isOpen = FALSE;
    }

    /* Unlock instance */
    SemaphoreP_post(pObj->pruInstLock);    

    if (status == SystemP_FAILURE) 
    {
        retStatus = PRUI2S_DRV_SERR_CLOSE;
    }
    
    return retStatus;
}

/* 
 *  Enables PRU I2S instance 
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *
 *  return: status code
 */
int32_t PRUI2S_enable(
    PRUI2S_Handle handle
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    int32_t status;
    int32_t retStatus = PRUI2S_DRV_SOK;
    
    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;
    
    status = PRUICSS_enableCore(pObj->pruIcssHandle, pSwipAttrs->pruInstId);
    if (status != SystemP_SUCCESS) {
        retStatus = PRUI2S_DRV_SERR_PRU_EN;
    }   
    
    return retStatus;
}

/* 
 *  Initializes IO Buffer with default parameters.
 *  Same IO Buffer stucture used for write and read.
 *
 *  Parameters:
 *      pIoBuf      Pointer to PRUI2S_IoBuf structure to initialize
 *
 *  return: none
 */
void PRUI2S_ioBufInit(
    PRUI2S_IoBuf *pIoBuf
)
{
    pIoBuf->ioBufAddr = NULL;
}

/* 
 *  Writes I2S Tx data buffer to current PRU FW I2S Tx buffer (ping or pong).
 *  I2S Tx data buffer in interleaved format, i.e. no format change.
 *      - PRU FW I2S Tx buffer is located in ICSS memory.
 *      - PRU FW I2S Tx buffer address & length are contained in FW build 
 *        (and SW IP attributes after call to PRUI2S_Init()).
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      pIoBuf      Pointer to PRUI2S_IoBuf structure to use for write
 *
 *  return: status code
 */
int32_t PRUI2S_write(
    PRUI2S_Handle handle, 
    PRUI2S_IoBuf *pIoBuf
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t *dstAddr;
    uint8_t txPingPongSel;
    uint16_t size;
    uint8_t regVal;
    int32_t retStatus = PRUI2S_DRV_SOK;   

    if (pIoBuf->ioBufAddr != NULL)
    {
        /* Get pointers */
        pCfg = (PRUI2S_Config *)handle;
        pObj = pCfg->object;
        pSwipAttrs = pCfg->attrs;
        
        /* Copy Tx data to ping or pong buffer */
        txPingPongSel = HW_RD_REG8(pSwipAttrs->baseAddr + FW_REG_TX_PING_PONG_SEL);
        txPingPongSel &= TX_PING_PONG_SEL_BF_TX_PING_PONG_SEL_MASK;
        size = pObj->prms.pingPongBufSz/2;
        dstAddr = (uint8_t *)pObj->txPingPongBuf;
        if (txPingPongSel == PING_PONG_SEL_PONG)
        {
            dstAddr += size;
        }
        memcpy(dstAddr, (uint8_t *)pIoBuf->ioBufAddr, size);

    #ifdef _DBG_TX_PP_CAP    
        gTxPpSelCapBuf[gTxPpCapBufIdx] = txPingPongSel;
        gTxPpSrcAddrCapBuf[gTxPpCapBufIdx] = (uint32_t)pIoBuf->ioBufAddr;
        gTxPpDstAddrCapBuf[gTxPpCapBufIdx] = (uint32_t)dstAddr;
        gTxPpCapBufIdx++;
        if (gTxPpCapBufIdx >= TX_PP_CAP_BUF_SZ)
        {
            gTxPpCapBufIdx = 0;
        }
    #endif   

        /* Set Tx buffer full status */
        regVal = HW_RD_REG8(pSwipAttrs->baseAddr + FW_REG_TX_PING_PONG_STAT);
        if (txPingPongSel == PING_PONG_SEL_PING)
        {
            regVal |= 1 << TX_PING_PONG_STAT_BF_TX_PING_STAT_SHIFT;
            HW_WR_REG8(pSwipAttrs->baseAddr + FW_REG_TX_PING_PONG_STAT, regVal);
        }
        else /* PING_PONG_SEL_PONG */
        {
            regVal |= 1 << TX_PING_PONG_STAT_BF_TX_PONG_STAT_SHIFT;
            HW_WR_REG8(pSwipAttrs->baseAddr + FW_REG_TX_PING_PONG_STAT, regVal);
        }
    }
    else
    {
        retStatus = PRUI2S_DRV_SERR_INV_PRMS;
    }

    return retStatus;
}
   
/* 
 *  Reads I2S Rx data buffer from current PRU FW I2S Rx buffer (ping or pong).
 *  I2S Rx data buffer in interleaved format, i.e. no format change.
 *      - PRU FW I2S Rx buffer location is determined by application.
 *      - PRU FW I2S Rx buffer length is contained in FW build 
 *        (and SW IP attributes after call to PRUI2S_Init()).
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      pIoBuf      Pointer to PRUI2S_IoBuf structure to use for read
 *
 *  return: status code
 */
int32_t PRUI2S_read(
    PRUI2S_Handle handle, 
    PRUI2S_IoBuf *pIoBuf
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t *srcAddr;
    uint8_t rxPingPongSel;
    uint16_t size;
    uint8_t regVal;
    int32_t retStatus = PRUI2S_DRV_SOK;   

    if (pIoBuf->ioBufAddr != NULL)
    {
        /* Get pointers */
        pCfg = (PRUI2S_Config *)handle;
        pObj = pCfg->object;
        pSwipAttrs = pCfg->attrs;
        
        /* Copy Rx data to ping or pong buffer */
        rxPingPongSel = HW_RD_REG8(pSwipAttrs->baseAddr + FW_REG_RX_PING_PONG_SEL);
        rxPingPongSel &= RX_PING_PONG_SEL_BF_RX_PING_PONG_SEL_MASK;
        size = pObj->prms.pingPongBufSz/2;
        srcAddr = (uint8_t *)pObj->rxPingPongBuf;
        if (rxPingPongSel == PING_PONG_SEL_PONG)
        {
            srcAddr += size;
        }
        memcpy((uint8_t *)pIoBuf->ioBufAddr, srcAddr, size);

    #ifdef _DBG_RX_PP_CAP    
        gRxPpSelCapBuf[gRxPpCapBufIdx] = rxPingPongSel;
        gRxPpSrcAddrCapBuf[gRxPpCapBufIdx] = (uint32_t)srcAddr;
        gRxPpDstAddrCapBuf[gRxPpCapBufIdx] = (uint32_t)pIoBuf->ioBufAddr;
        gRxPpCapBufIdx++;
        if (gRxPpCapBufIdx >= RX_PP_CAP_BUF_SZ)
        {
            gRxPpCapBufIdx = 0;
        }
    #endif   

        /* Set Rx buffer empty status */
        regVal = HW_RD_REG8(pSwipAttrs->baseAddr + FW_REG_RX_PING_PONG_STAT);
        if (rxPingPongSel == PING_PONG_SEL_PING)
        {
            regVal &= ~(1 << RX_PING_PONG_STAT_BF_RX_PING_STAT_SHIFT);
            HW_WR_REG8(pSwipAttrs->baseAddr + FW_REG_RX_PING_PONG_STAT, regVal);
        }
        else /* PING_PONG_SEL_PONG */
        {
            regVal &= ~(1 << RX_PING_PONG_STAT_BF_RX_PONG_STAT_SHIFT);
            HW_WR_REG8(pSwipAttrs->baseAddr + FW_REG_RX_PING_PONG_STAT, regVal);
        }
    }
    else
    {
        retStatus = PRUI2S_DRV_SERR_INV_PRMS;
    }
    
    return retStatus;
}

/* 
 *  Initializes conversion IO Buffer with default parameters. 
 *
 *  Parameters:
 *      pIoBuf      Pointer to PRUI2S_IoBufC structure to initialize
 *
 *  return: none
 */
void PRUI2S_ioBufCInit(
    PRUI2S_IoBufC *pIoBufC
)
{
    uint8_t i;
    
    for (i = 0; i < PRUI2S_MAX_I2S; i++)
    {
        pIoBufC->ioBufLAddr[i] = NULL;
        pIoBufC->ioBufRAddr[i] = NULL;
    }        
}

/* 
 *  Writes I2S Tx data buffers to current PRU FW I2S Tx buffer (ping or pong).
 *  I2S Tx data buffers in non-interleaved format, buffers are written to
 *  current I2S Tx buffer (ping or ping) in interleaved format.
 *      - PRU FW I2S Tx buffer is located in ICSS memory.
 *      - PRU FW I2S Tx buffer address & length are contained in FW build 
 *        (and SW IP attributes after call to PRUI2S_Init()).
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      pIoBufC     Pointer to PRUI2S_IoBufC structure to use for write
 *
 *  return: status code
 */
int32_t PRUI2S_writeC(
    PRUI2S_Handle handle, 
    PRUI2S_IoBufC *pIoBufC
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t numTxI2s;       /* number of Tx I2S */
    uint8_t numTxCh;        /* number of Tx channels */
    uint8_t bytesPerSlot;   /* number of Tx bytes per slot */
    uint16_t numSlots;      /* number of Tx slots */
    int16_t *pSrc16b, *pDst16b;
    int32_t *pSrc32b, *pDst32b;
    uint8_t i;
    uint16_t j;
    int32_t retStatus = PRUI2S_DRV_SOK;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    /* Get number of Tx I2S */
    numTxI2s = pSwipAttrs->numTxI2s;

    /* Check IO buffer parameter */
    for (i = 0; i < numTxI2s; i++)
    {
        if ((pIoBufC->ioBufLAddr[i] == NULL) || 
            (pIoBufC->ioBufRAddr[i] == NULL))
        {
            retStatus = PRUI2S_DRV_SERR_INV_PRMS;
            break;
        }
    }

    if (retStatus == PRUI2S_DRV_SOK)
    {
        /* Calculate bytes per slot, number of channels, number of slots */
        bytesPerSlot = pSwipAttrs->bitsPerSlot / 8;
        numTxCh = 2*numTxI2s;
        numSlots = pObj->prms.pingPongBufSz / (numTxCh * bytesPerSlot);

        if (pSwipAttrs->bitsPerSlot == PRUI2S_BITS_PER_SLOT_16)
        {
            /* Interleave Left channel data */
            for (i = 0; i < numTxI2s; i++)
            {
                pSrc16b = (int16_t *)pIoBufC->ioBufLAddr[i];
                pDst16b = (int16_t *)pObj->txPingPongBuf;
                pDst16b += i;
                
                for (j = 0; j < numSlots; j++)
                {
                    *pDst16b = *pSrc16b;
                    pSrc16b++;
                    pDst16b += numTxCh;
                }
            }

            /* Interleave Right channel data */
            for (i = 0; i < numTxI2s; i++)
            {
                pSrc16b = (int16_t *)pIoBufC->ioBufRAddr[i];
                pDst16b = (int16_t *)pObj->txPingPongBuf;
                pDst16b += numTxI2s+i;
                for (j = 0; j < numSlots; j++)
                {
                    *pDst16b = *pSrc16b;
                    pSrc16b++;
                    pDst16b += numTxCh;
                }        
            }
        }
        else if (pSwipAttrs->bitsPerSlot == PRUI2S_BITS_PER_SLOT_32)
        {
            /* Interleave Left channel data */
            for (i = 0; i < numTxI2s; i++)
            {
                pSrc32b = (int32_t *)pIoBufC->ioBufLAddr[i];
                pDst32b = (int32_t *)pObj->txPingPongBuf;
                pDst32b += i;
                for (j = 0; j < numSlots; j++)
                {
                    *pDst32b = *pSrc32b;
                    pSrc32b++;
                    pDst32b += numTxCh;
                }
            }

            /* Interleave Left channel data */
            for (i = 0; i < numTxI2s; i++)
            {
                /* Interleave Right channel data */
                pSrc32b = (int32_t *)pIoBufC->ioBufRAddr[i];
                pDst32b = (int32_t *)pObj->txPingPongBuf;
                pDst32b += numTxI2s+i;
                for (j = 0; j < numSlots; j++)
                {
                    *pDst32b = *pSrc32b;
                    pSrc32b++;
                    pDst32b += numTxCh;
                }        
            }
        }
    }
    
    return retStatus;
}
   
/* 
 *  Reads I2S Rx data buffer from current PRU FW I2S Rx buffer (ping or pong).
 *  I2S Rx data buffers in non-interleaved format, buffers are read from
 *  current I2S Rx buffer (ping or ping) in interleaved format.
 *      - PRU FW I2S Rx buffer location is determined by application.
 *      - PRU FW I2S Rx buffer length is contained in FW build 
 *        (and SW IP attributes after call to PRUI2S_Init()).
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      pIoBufC     Pointer to PRUI2S_IoBufC structure to use for read
 *
 *  return: status code
 */
int32_t PRUI2S_readC(
    PRUI2S_Handle handle, 
    PRUI2S_IoBufC *pIoBufC
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t numRxI2s;       /* number of Rx I2S */
    uint8_t numRxCh;        /* number of Rx channels */
    uint8_t bytesPerSlot;   /* number of Rx bytes per slot */
    uint16_t numSlots;      /* number of Rx slots */
    int16_t *pSrc16b, *pDst16b;
    int32_t *pSrc32b, *pDst32b;
    uint8_t i;
    uint16_t j;
    int32_t retStatus = PRUI2S_DRV_SOK;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    /* Get number of Rx I2S */
    numRxI2s = pSwipAttrs->numRxI2s;

    /* Check IO buffer parameter */
    for (i = 0; i < numRxI2s; i++)
    {
        if ((pIoBufC->ioBufLAddr[i] == NULL) || 
            (pIoBufC->ioBufRAddr[i] == NULL))
        {
            retStatus = PRUI2S_DRV_SERR_INV_PRMS;
            break;
        }
    }

    if (retStatus == PRUI2S_DRV_SOK)
    {
        /* Calculate bytes per slot, number of channels, number of slots */
        bytesPerSlot = pSwipAttrs->bitsPerSlot / 8;
        numRxCh = 2*numRxI2s;
        numSlots = pObj->prms.pingPongBufSz / (numRxCh * bytesPerSlot);

        if (pSwipAttrs->bitsPerSlot == PRUI2S_BITS_PER_SLOT_16)
        {
            /* Interleave Left channel data */
            for (i = 0; i < numRxI2s; i++)
            {
                pSrc16b = (int16_t *)pObj->rxPingPongBuf;
                pSrc16b += i;            
                pDst16b = (int16_t *)pIoBufC->ioBufLAddr[i];
                for (j = 0; j < numSlots; j++)
                {
                    *pDst16b = *pSrc16b;
                    pSrc16b += numRxCh;
                    pDst16b++;
                }
            }

            /* Interleave Right channel data */
            for (i = 0; i < numRxI2s; i++)
            {
                pSrc16b = (int16_t *)pObj->rxPingPongBuf;
                pSrc16b += numRxI2s+i;
                pDst16b = (int16_t *)pIoBufC->ioBufRAddr[i];
                for (j = 0; j < numSlots; j++)
                {
                    *pDst16b = *pSrc16b;
                    pSrc16b += numRxCh;
                    pDst16b++;
                }        
            }
        }
        else if (pSwipAttrs->bitsPerSlot == PRUI2S_BITS_PER_SLOT_32)
        {
            /* Interleave Left channel data */
            for (i = 0; i < numRxI2s; i++)
            {
                pSrc32b = (int32_t *)pObj->rxPingPongBuf;
                pSrc32b += i;            
                pDst32b = (int32_t *)pIoBufC->ioBufLAddr[i];
                for (j = 0; j < numSlots; j++)
                {
                    *pDst32b = *pSrc32b;
                    pSrc32b += numRxCh;
                    pDst32b++;
                }
            }

            /* Interleave Right channel data */
            for (i = 0; i < numRxI2s; i++)
            {
                pSrc32b = (int32_t *)pObj->rxPingPongBuf;
                pSrc32b += numRxI2s+i;
                pDst32b = (int32_t *)pIoBufC->ioBufRAddr[i];
                for (j = 0; j < numSlots; j++)
                {
                    *pDst32b = *pSrc32b;
                    pSrc32b += numRxCh;
                    pDst32b++;
                }        
            }
        }
    }
    
    return retStatus;
}

/* 
 *  Gets error status.
 *  Application calls this function to obtain error status and 
 *  determine cause of errors.
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      pErrStat    Pointer to errot status bit-field.
 *
 *  return: none
 */
void PRUI2S_getErrStat(
    PRUI2S_Handle handle, 
    uint8_t *pErrStat
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t regVal;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pSwipAttrs = pCfg->attrs;
    
    regVal = HW_RD_REG8(pSwipAttrs->baseAddr + FW_REG_ERR_STAT);
    *pErrStat = regVal;
}

/* 
 *  Clears error status.
 *  Application calls this function to clear error status.
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      pErrStat    Pointer to errot status bit-field mask.
 *
 *  return: none
 */
void PRUI2S_clearErrStat(
    PRUI2S_Handle handle, 
    uint8_t errMask
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t regVal;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pSwipAttrs = pCfg->attrs;

    regVal = HW_RD_REG8(pSwipAttrs->baseAddr + FW_REG_ERR_STAT);
    regVal &= ~errMask;
    HW_WR_REG8(pSwipAttrs->baseAddr + FW_REG_ERR_STAT, regVal);
}

/* 
 *  Disables PRUI2S interrupt
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      intrType    Type of interrupt to disable
 *
 *  return: status code
 */
int32_t PRUI2S_disableInt(
    PRUI2S_Handle handle, 
    uint8_t intrType
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t icssIntcSysEvt;
    int32_t status;
    int32_t retStatus = PRUI2S_DRV_SOK;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    /* Get system event for interrupt type */
    if (intrType == INTR_TYPE_I2S_TX)
    {
        icssIntcSysEvt = pSwipAttrs->i2sTxIcssIntcSysEvt;
    }
    else if (intrType == INTR_TYPE_I2S_RX)
    {
        icssIntcSysEvt = pSwipAttrs->i2sRxIcssIntcSysEvt;
    }
    else if (intrType == INTR_TYPE_I2S_ERR)
    {
        icssIntcSysEvt = pSwipAttrs->i2sErrIcssIntcSysEvt;
    }
    else
    {
        retStatus = PRUI2S_DRV_SERR_INV_PRMS;
    }
    
    if (retStatus == PRUI2S_DRV_SOK)
    {
        status = PRUICSS_disableEvent(pObj->pruIcssHandle, icssIntcSysEvt);
        if (status != SystemP_SUCCESS) {
            retStatus = PRUI2S_DRV_SERR_CLR_INT;
        }      
    }

    return retStatus;
}

/* 
 *  Enables PRUI2S interrupt 
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      intrType    Type of interrupt to disable
 *
 *  return: status code
 */
int32_t PRUI2S_enableInt(
    PRUI2S_Handle handle, 
    uint8_t intrType
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t icssIntcSysEvt;
    int32_t status;
    int32_t retStatus = PRUI2S_DRV_SOK;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    /* Get system event for interrupt type */
    if (intrType == INTR_TYPE_I2S_TX)
    {
        icssIntcSysEvt = pSwipAttrs->i2sTxIcssIntcSysEvt;
    }
    else if (intrType == INTR_TYPE_I2S_RX)
    {
        icssIntcSysEvt = pSwipAttrs->i2sRxIcssIntcSysEvt;
    }
    else if (intrType == INTR_TYPE_I2S_ERR)
    {
        icssIntcSysEvt = pSwipAttrs->i2sErrIcssIntcSysEvt;
    }
    else
    {
        retStatus = PRUI2S_DRV_SERR_INV_PRMS;
    }
    
    if (retStatus == PRUI2S_DRV_SOK)
    {
        status = PRUICSS_enableEvent(pObj->pruIcssHandle, icssIntcSysEvt);
        if (status != SystemP_SUCCESS) {
            retStatus = PRUI2S_DRV_SERR_CLR_INT;
        }      
    }

    return retStatus;
}

/* 
 *  Clears PRUI2S interrupt 
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      intrType    Type of interrupt to disable
 *
 *  return: status code
 */
int32_t PRUI2S_clearInt(
    PRUI2S_Handle handle, 
    uint8_t intrType
)
{
    PRUI2S_Config *pCfg;
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t icssIntcSysEvt;
    int32_t status;
    int32_t retStatus = PRUI2S_DRV_SOK;

    /* Get pointers */
    pCfg = (PRUI2S_Config *)handle;
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    /* Get system event for interrupt type */
    if (intrType == INTR_TYPE_I2S_TX)
    {
        icssIntcSysEvt = pSwipAttrs->i2sTxIcssIntcSysEvt;
    }
    else if (intrType == INTR_TYPE_I2S_RX)
    {
        icssIntcSysEvt = pSwipAttrs->i2sRxIcssIntcSysEvt;
    }
    else if (intrType == INTR_TYPE_I2S_ERR)
    {
        icssIntcSysEvt = pSwipAttrs->i2sErrIcssIntcSysEvt;
    }
    else
    {
        retStatus = PRUI2S_DRV_SERR_INV_PRMS;
    }
    
    if (retStatus == PRUI2S_DRV_SOK)
    {
        status = PRUICSS_clearEvent(pObj->pruIcssHandle, icssIntcSysEvt);
        if (status != SystemP_SUCCESS) {
            retStatus = PRUI2S_DRV_SERR_CLR_INT;
        }      
    }

    return retStatus;
}

/* Extracts information in PRU FW pseudo-registers, update SW IP using extracted info.
   Info is contained in PRU FW DMEM image. */
static int32_t PRUI2S_getPruFwImageInfo(
    PRUI2S_PruFwImageInfo *pPruFwImageInfo, 
    PRUI2S_SwipAttrs *pSwipAttrs
)
{
    const uint8_t *pPruDmemImg = pPruFwImageInfo->pPruDmemImg;
    uint8_t temp8b;
    uint8_t i;
    
    /* Update number of Tx I2S */
    temp8b = pPruDmemImg[FW_REG_NUM_TX_I2S_OFFSET];
    pSwipAttrs->numTxI2s = temp8b;
    
    /* Update number of Rx I2S */
    temp8b = pPruDmemImg[FW_REG_NUM_RX_I2S_OFFSET];
    pSwipAttrs->numRxI2s = temp8b;
    
    /* Update sampling frequency */
    temp8b = pPruDmemImg[FW_REG_SAMP_FREQ_OFFSET];
    pSwipAttrs->sampFreq = temp8b;
    
    /* Update number if bits per I2S slot */
    temp8b = pPruDmemImg[FW_REG_BITS_PER_SLOT_OFFSET];
    pSwipAttrs->bitsPerSlot = temp8b;
    
    /* Update I2S Tx ICSS INTC system event number */
    temp8b = pPruDmemImg[FW_REG_I2S_TX_ICSS_INTC_SYS_EVT_OFFSET];
    pSwipAttrs->i2sTxIcssIntcSysEvt = temp8b;
    
    /* Update I2S Rx ICSS INTC system event number */
    temp8b = pPruDmemImg[FW_REG_I2S_RX_ICSS_INTC_SYS_EVT_OFFSET];
    pSwipAttrs->i2sRxIcssIntcSysEvt = temp8b;

    /* Update I2S error ICSS INTC system event number */
    temp8b = pPruDmemImg[FW_REG_I2S_ERR_ICSS_INTC_SYS_EVT_OFFSET];
    pSwipAttrs->i2sErrIcssIntcSysEvt = temp8b;
    
    /* 
     * Update pin numbers 
     */

    /* Update BCLK */
    temp8b = pPruDmemImg[FW_REG_I2S_PIN_NUM_BCLK_OFFSET];
    pSwipAttrs->bclkPin.pinNum = temp8b;
    
    /* Update FSYNC */
    temp8b = pPruDmemImg[FW_REG_I2S_PIN_NUM_FSYNC_OFFSET];
    pSwipAttrs->fsyncPin.pinNum = temp8b;
    
    /* Update Tx pins */
    for (i = 0; i < pSwipAttrs->numTxI2s; i++)
    {
        temp8b = pPruDmemImg[FW_REG_I2S_PIN_NUM_TX0_OFFSET+i];
        pSwipAttrs->txPin[i].pinNum = temp8b;
    }
    
    /* Update Rx pins */    
    for (i = 0; i < pSwipAttrs->numRxI2s; i++)
    {
        temp8b = pPruDmemImg[FW_REG_I2S_PIN_NUM_RX0_OFFSET+i];
        pSwipAttrs->rxPin[i].pinNum = temp8b;
    }

    /* TBD: SoC PAD address lookup */

    return SystemP_SUCCESS;
}

/* Checks SW IP parameters */
static int32_t PRUI2S_checkSwipParams(
    PRUI2S_SwipAttrs *pSwipAttrs
)
{
    const PRUICSS_HwAttrs *pPruIcssHwAttrs;
    uint32_t icssHwInstId;
    int32_t status = SystemP_SUCCESS;
    uint8_t i;

    pPruIcssHwAttrs = PRUICSS_getAttrs(pSwipAttrs->icssInstId);
    icssHwInstId = pPruIcssHwAttrs->instance;

    /* Check base address */
    if (pSwipAttrs->baseAddr == 0)
    {
        status = SystemP_FAILURE;
    }

    /* Check PRU core ID */
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->pruInstId != PRUICSS_PRU0) && (pSwipAttrs->pruInstId != PRUICSS_PRU1)))
    {
        status = SystemP_FAILURE;
    }
    
    /* Check number of I2S instance */
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->numTxI2s == 0) && (pSwipAttrs->numRxI2s == 0)))
    {
        status = SystemP_FAILURE;
    }
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->numTxI2s != 0) && (pSwipAttrs->numRxI2s != 0)) &&
        (pSwipAttrs->numTxI2s != pSwipAttrs->numRxI2s))
    {
        status = SystemP_FAILURE;       
    }
    
    /* Check bits per I2S slot */
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->bitsPerSlot != PRUI2S_BITS_PER_SLOT_16) &&
        (pSwipAttrs->bitsPerSlot != PRUI2S_BITS_PER_SLOT_32)))
    {
        status = SystemP_FAILURE;       
    }

    /* 
       Check Host interrupt numbers.
       Note R5F interrupt ranges for ICSS host interrupts are all the same. 
     */
    if ((status == SystemP_SUCCESS) &&
        (icssHwInstId == PRUICSS_INSTANCE_ONE) &&
        ((pSwipAttrs->i2sTxHostIntNum < CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_0)   ||
        (pSwipAttrs->i2sTxHostIntNum > CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_7)    ||
        (pSwipAttrs->i2sRxHostIntNum < CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_0)    ||
        (pSwipAttrs->i2sRxHostIntNum > CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_7)    ||
        (pSwipAttrs->i2sErrHostIntNum < CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_0) ||
        (pSwipAttrs->i2sErrHostIntNum > CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_7)))
    {
            status = SystemP_FAILURE;       
    }

    /* Check ICSS system event numbers */
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->i2sTxIcssIntcSysEvt < PRU_ARM_EVENT00) || 
        (pSwipAttrs->i2sTxIcssIntcSysEvt > PRU_ARM_EVENT15)))
    {
        status = SystemP_FAILURE;       
    }
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->i2sRxIcssIntcSysEvt < PRU_ARM_EVENT00) || 
        (pSwipAttrs->i2sRxIcssIntcSysEvt > PRU_ARM_EVENT15)))
    {
        status = SystemP_FAILURE;       
    }
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->i2sErrIcssIntcSysEvt < PRU_ARM_EVENT00) || 
        (pSwipAttrs->i2sErrIcssIntcSysEvt > PRU_ARM_EVENT15)))
    {
        status = SystemP_FAILURE;
    }
    
    /* Check PRU pin numbers */
    if ((status == SystemP_SUCCESS) &&
        ((pSwipAttrs->bclkPin.pinNum > PRUI2S_MAX_PRU_PIN_NUM) || 
         (pSwipAttrs->fsyncPin.pinNum > PRUI2S_MAX_PRU_PIN_NUM)))
    {
        status = SystemP_FAILURE;
    }    
    if (status == SystemP_SUCCESS)
    {
        for (i = 0; i < pSwipAttrs->numTxI2s; i++)
        {
            if (pSwipAttrs->txPin[i].pinNum > PRUI2S_MAX_PRU_PIN_NUM)
            {
                status = SystemP_FAILURE;
                break;
            }
        }
    }
    if (status == SystemP_SUCCESS)
    {
        for (i = 0; i < pSwipAttrs->numRxI2s; i++)
        {
            if (pSwipAttrs->rxPin[i].pinNum > PRUI2S_MAX_PRU_PIN_NUM)
            {
                status = SystemP_FAILURE;
                break;
            }
        }
    }
    
    return status;
}

/* Checks parameters used for PRUI2S_open() function */
static int32_t PRUI2S_checkOpenParams(
    PRUI2S_SwipAttrs *pSwipAttrs,
    PRUI2S_Params *pPrms
)
{
    const PRUICSS_HwAttrs *pPruIcssHwAttrs;
    uint32_t icssHwInstId;
    int32_t status = SystemP_SUCCESS;
    
    pPruIcssHwAttrs = PRUICSS_getAttrs(pSwipAttrs->icssInstId);
    icssHwInstId = pPruIcssHwAttrs->instance;    
    
    /* Check Tx I2S parameters */
    if (pSwipAttrs->numTxI2s > 0)
    {
        /* Check Tx interrupt priority */
        if (pPrms->i2sTxIntrPri > MAX_VIM_INTR_PRI_VAL)
        {
            status = SystemP_FAILURE;
        }

        /* Check Tx interrupt callback function */
        if ((status == SystemP_SUCCESS) && (pPrms->i2sTxCallbackFxn == NULL))
        {
            status = SystemP_FAILURE;
        }
        
        /* Check Tx ping/pong buffer base address.
           Tx ping/pong buffer is expected to be in ICSS SHMEM. */
        if ((status == SystemP_SUCCESS) && 
            (icssHwInstId == PRUICSS_INSTANCE_ONE) && 
            ((pPrms->txPingPongBaseAddr < CSL_ICSS_M_RAM_SLV_RAM_REGS_BASE) ||
             (pPrms->txPingPongBaseAddr >= (CSL_ICSS_M_RAM_SLV_RAM_REGS_BASE + sizeof(CSL_icss_m_ram_slv_ramRegs)))))
        {
            status = SystemP_FAILURE;
        }
    }
    
    /* Check Rx I2S parameters */
    if ((status == SystemP_SUCCESS) && (pSwipAttrs->numRxI2s > 0))
    {
        /* Check Rx interrupt priority */
        if (pPrms->i2sRxIntrPri > MAX_VIM_INTR_PRI_VAL)
        {
            status = SystemP_FAILURE;
        }

        /* Check Rx interrupt callback function */
        if ((status == SystemP_SUCCESS) && (pPrms->i2sRxCallbackFxn == NULL))
        {
            status = SystemP_FAILURE;
        }

        /* Check Rx ping/pong buffer base address */
        if ((status == SystemP_SUCCESS) && (pPrms->rxPingPongBaseAddr == 0))
        {
            status = SystemP_FAILURE;
        }
    }
    
    /* Check ping/pong buffer size */
    if ((status == SystemP_SUCCESS) &&
        (pPrms->pingPongBufSz == 0))
    {
        status = SystemP_FAILURE;
    }        

    /* Check I2S error parameters */
    if ((status == SystemP_SUCCESS) && (pPrms->i2sErrCallbackFxn != NULL))
    {
        /* Check I2S error interrupt priority */
        if (pPrms->i2sErrIntrPri > MAX_VIM_INTR_PRI_VAL)
        {
            status = SystemP_FAILURE;
        }
    }
    
    return status;
}

/* Initializes PRU for PRU I2S */
static int32_t PRUI2S_initPru(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    PRUI2S_Params *pPrms;
    uint8_t icssInstId;
    uint8_t pruInstId;
    PRUICSS_Handle pruIcssHandle;
    int32_t size;
    const uint32_t *sourceMem;    /* Source memory[ Array of uint32_t ] */
    uint32_t offset;              /* Offset at which write will happen */
    uint32_t byteLen;             /* Total number of bytes to be written */
    int32_t status = SystemP_SUCCESS;   
    
    /* Get pointers */
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;
    pPrms = &pCfg->object->prms;
    if (pPrms == NULL)
    {
        status = SystemP_FAILURE;
    }

    icssInstId = pSwipAttrs->icssInstId; /* get ICSS instance ID */
    pruInstId = pSwipAttrs->pruInstId;   /* get PRU instance ID */
    
    /* Create PRUSS driver */
    pruIcssHandle = PRUICSS_open(icssInstId);
    if (pruIcssHandle == NULL)
    {
        status = SystemP_FAILURE;
    }
    
    if (status == SystemP_SUCCESS)
    {
        /* Disable PRU core */
        status = PRUICSS_disableCore(pruIcssHandle, pruInstId);
    }
    
    if (status == SystemP_SUCCESS)
    {
        /* Set ICSS pin mux */
        status = PRUICSS_setSaMuxMode(pruIcssHandle, PRUICSS_G_MUX_EN);
    }
    
    if (status == SystemP_SUCCESS)
    {
        /* Reset PRU core */
        status = PRUICSS_resetCore(pruIcssHandle, pruInstId);
    }

    if (status == SystemP_SUCCESS)
    {    
        /* Initialize DMEM to 0 */
        size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_DATARAM(pruInstId));
        if (size == 0)
        {
            status = SystemP_FAILURE;
        }
    }
    
    if (status == SystemP_SUCCESS)
    {    
        /* Initialize IMEM to 0 */
        size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_IRAM_PRU(pruInstId));
        if (size == 0)
        {
            status = SystemP_FAILURE;
        }
    }
    
    if ((status == SystemP_SUCCESS) && (pObj->pPruFwImageInfo->pPruDmemImg != NULL))
    {
        /* Write DMEM */
        offset = 0;
        sourceMem = (uint32_t *)pObj->pPruFwImageInfo->pPruDmemImg;
        byteLen = pObj->pPruFwImageInfo->pruDmemImgSz;
        size = PRUICSS_writeMemory(pruIcssHandle, PRUICSS_DATARAM(pruInstId), offset, sourceMem, byteLen);
        if (size == 0)
        {
            status = SystemP_FAILURE;
        }        
    }
    
    if (status == SystemP_SUCCESS)
    {    
        /* Write IMEM */
        offset = 0;
        sourceMem = (uint32_t *)pObj->pPruFwImageInfo->pPruImemImg;
        byteLen = pObj->pPruFwImageInfo->pruImemImgSz;
        size = PRUICSS_writeMemory(pruIcssHandle, PRUICSS_IRAM_PRU(pruInstId), offset, sourceMem, byteLen);
        if (size == 0)
        {
            status = SystemP_FAILURE;
        }
        else
        {
            /* Set Object PRUICSS handle */
            pObj->pruIcssHandle = pruIcssHandle;
        }
    }
    
    return status;
}

/* Initializes ICSS INTC for PRU I2S */
static int32_t PRUI2S_initIcssIntc(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    PRUI2S_IcssIntcCfgInfo *pPruIcssIntcCfgInfo;
    PRUICSS_IntcInitData intcInitData;
    uint8_t i, j;
    int32_t status = SystemP_SUCCESS;

    /* Get pointers */
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    if (gIcssIntcInitFlag[pSwipAttrs->icssInstId] == FALSE)
    {
        /* 
         * Initialize INTC 
         */
        
        /* Get pointer to PRU ICSS INTC configuration info */
        pPruIcssIntcCfgInfo = &gPruIcssIntcCfgInfo[pSwipAttrs->icssInstId][pSwipAttrs->pruInstId];
        
        /* Record SWIP system events in INTC configuration info */
        if (pSwipAttrs->numTxI2s > 0)
        {
            pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT0_IDX] = TRUE;
            pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT0_IDX] = pSwipAttrs->i2sTxIcssIntcSysEvt;
            pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT0_IDX].sysevt = pSwipAttrs->i2sTxIcssIntcSysEvt;
        }
        if (pSwipAttrs->numRxI2s > 0)
        {
            pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT1_IDX] = TRUE;
            pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT1_IDX] = pSwipAttrs->i2sRxIcssIntcSysEvt;
            pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT1_IDX].sysevt = pSwipAttrs->i2sRxIcssIntcSysEvt;
        }
        pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT2_IDX] = TRUE;
        pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT2_IDX] = pSwipAttrs->i2sErrIcssIntcSysEvt;
        pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT2_IDX].sysevt = pSwipAttrs->i2sErrIcssIntcSysEvt;

        /* 
         *   Set INTC init data 
         */
        /* Reset INTC data */
        intcInitData = (PRUICSS_IntcInitData)PRUICSS_INTC_INITDATA;
        /* Set enabled system events, 
           set system event to channel map */
        j = 0;
        for (i = 0; i < PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU; i++)
        {
            if (pPruIcssIntcCfgInfo->sysevtEnabled[i] == TRUE)
            {
                intcInitData.sysevtsEnabled[j] = pPruIcssIntcCfgInfo->sysevtId[i];
                intcInitData.sysevtToChannelMap[j] = pPruIcssIntcCfgInfo->sysevtToChannelMap[i];            
                j++;
            }
        }
        /* Set channel to host interrupt map */
        for (i = 0; i < PRUI2S_PRUICSS_INTC_NUM_CHANNELS_PER_PRU; i++)
        {
            intcInitData.channelToHostMap[i] = pPruIcssIntcCfgInfo->channelToHostMap[i];
        }
        /* Set host enable bit mask */
        intcInitData.hostEnableBitmask = pPruIcssIntcCfgInfo->hostEnableBitmask;

        /* Initialize INTC */
        status = PRUICSS_intcInit(pObj->pruIcssHandle, &intcInitData);
        
        if (status == SystemP_SUCCESS)
        {
            /* Mark INTC initialization done */
            gIcssIntcInitFlag[pSwipAttrs->icssInstId] = TRUE;
            /* Mark INTC initialization for this PRU done */
            pPruIcssIntcCfgInfo->init = TRUE;
        }
    }
    else
    {
        /*
         * INTC already initialized
         */
        
        pPruIcssIntcCfgInfo = &gPruIcssIntcCfgInfo[pSwipAttrs->icssInstId][pSwipAttrs->pruInstId];
        if (pPruIcssIntcCfgInfo->init == FALSE)
        {
            /*
             *  Configure INTC for this PRU
             */
            
            /* Record SWIP system events in INTC configuration info */
            if (pSwipAttrs->numTxI2s > 0)
            {
                pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT0_IDX] = TRUE;
                pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT0_IDX] = pSwipAttrs->i2sTxIcssIntcSysEvt;
                pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT0_IDX].sysevt = pSwipAttrs->i2sTxIcssIntcSysEvt;
            }
            if (pSwipAttrs->numRxI2s > 0)
            {
                pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT1_IDX] = TRUE;
                pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT1_IDX] = pSwipAttrs->i2sRxIcssIntcSysEvt;
                pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT1_IDX].sysevt = pSwipAttrs->i2sRxIcssIntcSysEvt;
            }
            pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT2_IDX] = TRUE;
            pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT2_IDX] = pSwipAttrs->i2sErrIcssIntcSysEvt;
            pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT2_IDX].sysevt = pSwipAttrs->i2sErrIcssIntcSysEvt;

            /* 
             *   Set INTC init data 
             */
            /* Reset INTC data */
            intcInitData = (PRUICSS_IntcInitData)PRUICSS_INTC_INITDATA;
            /* Set enabled system events, 
               set system event to channel map */
            j = 0;
            for (i = 0; i < PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU; i++)
            {
                if (pPruIcssIntcCfgInfo->sysevtEnabled[i] == TRUE)
                {
                    intcInitData.sysevtsEnabled[j] = pPruIcssIntcCfgInfo->sysevtId[i];
                    intcInitData.sysevtToChannelMap[j] = pPruIcssIntcCfgInfo->sysevtToChannelMap[i];
                    j++;
                }
            }
            /* Set channel to host interrupt map */
            for (i = 0; i < PRUI2S_PRUICSS_INTC_NUM_CHANNELS_PER_PRU; i++)
            {
                intcInitData.channelToHostMap[i] = pPruIcssIntcCfgInfo->channelToHostMap[i];
            }
            /* Set host enable bit mask */
            intcInitData.hostEnableBitmask = pPruIcssIntcCfgInfo->hostEnableBitmask;

            /* Configure INTC for this PRU */
            status = PRUICSS_intcCfg(pObj->pruIcssHandle, &intcInitData);

            if (status == SystemP_SUCCESS)
            {
                /* Mark INTC initialized for this PRU */
                pPruIcssIntcCfgInfo->init = TRUE;
            }
        }
        else
        {
            /* 
             * Reconfigure INTC for this PRU 
             */
            
            /* Disable existing system events */
            j = 0;
            for (i = 0; i < PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU; i++)
            {
                if (pPruIcssIntcCfgInfo->sysevtEnabled[i] == TRUE)
                {
                    pPruIcssIntcCfgInfo->sysevtEnabled[j] = FALSE;

                    status = PRUICSS_disableEvent(pObj->pruIcssHandle, pPruIcssIntcCfgInfo->sysevtId[j]);
                    if (status == SystemP_FAILURE)
                    {
                        break;
                    }
                    status = PRUICSS_intcSetSysEvtChMap(pObj->pruIcssHandle, pPruIcssIntcCfgInfo->sysevtToChannelMap[j].sysevt, 0);
                    if (status == SystemP_FAILURE)
                    {
                        break;
                    }
                    j++;
                }
            }
            
            if (status == SystemP_SUCCESS)
            {
                /* Record SWIP system events in INTC configuration info */
                if (pSwipAttrs->numTxI2s > 0)
                {
                    pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT0_IDX] = TRUE;
                    pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT0_IDX] = pSwipAttrs->i2sTxIcssIntcSysEvt;
                    pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT0_IDX].sysevt = pSwipAttrs->i2sTxIcssIntcSysEvt;
                }
                if (pSwipAttrs->numRxI2s > 0)
                {
                    pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT1_IDX] = TRUE;
                    pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT1_IDX] = pSwipAttrs->i2sRxIcssIntcSysEvt;
                    pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT1_IDX].sysevt = pSwipAttrs->i2sRxIcssIntcSysEvt;
                }
                pPruIcssIntcCfgInfo->sysevtEnabled[PRUI2S_PRU_INTC_SYSEVT2_IDX] = TRUE;
                pPruIcssIntcCfgInfo->sysevtId[PRUI2S_PRU_INTC_SYSEVT2_IDX] = pSwipAttrs->i2sErrIcssIntcSysEvt;
                pPruIcssIntcCfgInfo->sysevtToChannelMap[PRUI2S_PRU_INTC_SYSEVT2_IDX].sysevt = pSwipAttrs->i2sErrIcssIntcSysEvt;
            }
            
            /* Update system event to channel map */
            j = 0;
            for (i = 0; i < PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU; i++)
            {
                if (pPruIcssIntcCfgInfo->sysevtEnabled[i] == TRUE)
                {
                    status = PRUICSS_intcSetSysEvtChMap(pObj->pruIcssHandle, pPruIcssIntcCfgInfo->sysevtToChannelMap[j].sysevt, 
                        pPruIcssIntcCfgInfo->sysevtToChannelMap[j].channel);
                    if (status == SystemP_FAILURE)
                    {
                        break;
                    }
                    status = PRUICSS_enableEvent(pObj->pruIcssHandle, pPruIcssIntcCfgInfo->sysevtId[j]);
                    if (status == SystemP_FAILURE)
                    {
                        break;
                    }
                    j++;
                }
            }
        }
    }
    
    return status;
}

/* Initializes PRU I2S FW */
static int32_t PRUI2S_initFw(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint32_t tmpRxPpBufAddr;

    /* Get pointers */
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    if (pSwipAttrs->numTxI2s > 0)
    {
        /* Write Tx ping/pong buffer address */
        HW_WR_REG32(pSwipAttrs->baseAddr + FW_REG_TX_PING_PONG_BUF_ADDR, pObj->prms.txPingPongBaseAddr);
    }

    if (pSwipAttrs->numRxI2s > 0)
    {
        /* Check if Rx PP buffer is located in ICSSM SHMEM. 
           If so, adjust address to be local to ICSSM. */
        tmpRxPpBufAddr = pObj->prms.rxPingPongBaseAddr;
        if ((pObj->prms.rxPingPongBaseAddr >= (CSL_ICSSM0_INTERNAL_U_BASE + CSL_ICSS_M_RAM_SLV_RAM_REGS_BASE)) &&
            (pObj->prms.rxPingPongBaseAddr < (CSL_ICSSM0_INTERNAL_U_BASE + CSL_ICSS_M_RAM_SLV_RAM_REGS_BASE + sizeof(CSL_icss_m_ram_slv_ramRegs))))
        {
            tmpRxPpBufAddr -= CSL_ICSSM0_INTERNAL_U_BASE;
        }
        
        /* Write Rx ping/pong buffer address */
        HW_WR_REG32(pSwipAttrs->baseAddr + FW_REG_RX_PING_PONG_BUF_ADDR, tmpRxPpBufAddr);
    }
    
    /* Write ping/pong buffer size */
    HW_WR_REG16(pSwipAttrs->baseAddr + FW_REG_PING_PONG_BUF_SZ, pObj->prms.pingPongBufSz);
    
    /* TBD: other FW reg init */
    
    return SystemP_SUCCESS;
}

/* Initializes PRU I2S ping/pong buffers */
static int32_t PRUI2S_initPpBufs(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    const PRUICSS_HwAttrs *pPruIcssHwAttrs;
    int32_t status = SystemP_SUCCESS;

    /* Get pointers */
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;

    if (pSwipAttrs->numTxI2s > 0)
    {
        pPruIcssHwAttrs = PRUICSS_getAttrs(pSwipAttrs->icssInstId);
        if (pPruIcssHwAttrs != NULL)
        {
            /* Initialize Object SoC Tx ping/pong address */
            if (pPruIcssHwAttrs->instance == PRUICSS_INSTANCE_ONE)
            {
                pObj->txPingPongBuf = (void *)(CSL_ICSSM0_INTERNAL_U_BASE + CSL_ICSS_M_DRAM0_SLV_RAM_REGS_BASE + pObj->prms.txPingPongBaseAddr);
            }
            else
            {
                status = SystemP_FAILURE;
            }
        }
        else
        {
            status = SystemP_FAILURE;
        }
        
        if (status == SystemP_SUCCESS)
        {
            /* Clear Tx ping/pong buffer */
            memset((uint8_t *)pObj->txPingPongBuf, 0, pObj->prms.pingPongBufSz);
        }
    }

    if ((status == SystemP_SUCCESS) && (pSwipAttrs->numRxI2s > 0))
    {
        /* Initialize Object Rx ping/pong pointer */
        pObj->rxPingPongBuf = (void *)pObj->prms.rxPingPongBaseAddr;

        /* Clear Rx ping/pong buffer */
        memset((uint8_t *)pObj->rxPingPongBuf, 0, pObj->prms.pingPongBufSz);
    }
    
    return status;
}

/* Configures SoC PADs for PRU I2S FW  */
static void PRUI2S_initFwPadRegs(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_SwipAttrs *pSwipAttrs;
    uint8_t i;
    
    /* Unlock pinmux registers */
    Pinmux_unlockMMR(PINMUX_DOMAIN_ID_MAIN);

    /* Get pointers */
    pSwipAttrs = pCfg->attrs;
    
    /* Initialize BCLK PAD register */
    PRUI2S_pruGpioPadConfig(&pSwipAttrs->bclkPin);
    
    /* Initialize FSYNC PAD register */
    PRUI2S_pruGpioPadConfig(&pSwipAttrs->fsyncPin);
    
    /* Initialize Tx I2S PAD registers */
    for (i = 0; i < pSwipAttrs->numTxI2s; i++)
    {
        PRUI2S_pruGpioPadConfig(&pSwipAttrs->txPin[i]);
    }
    
    /* Initialize Rx I2S PAD registers */
    for (i = 0; i < pSwipAttrs->numRxI2s; i++)
    {
        PRUI2S_pruGpioPadConfig(&pSwipAttrs->rxPin[i]);
    }
    
    /* Lock pinmux registers */
    Pinmux_lockMMR(PINMUX_DOMAIN_ID_MAIN);    
}

/* Configures PAD register */
static void PRUI2S_pruGpioPadConfig(
    PRUI2S_PruGpioPadAttrs *pPruGpioPadAttrs
)
{
    HW_WR_REG32(pPruGpioPadAttrs->padCfgRegAddr, pPruGpioPadAttrs->padCfgRegVal);
}

/* Configures PRU GPIO Output Control register PRU I2S FW  */
static int32_t PRUI2S_initPruGpioOutCtrlReg(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_SwipAttrs *pSwipAttrs;
    volatile uint32_t *pPruGpioOutCtrlReg;
    uint32_t regVal;
    uint32_t mask;
    uint8_t i;
    int32_t status = SystemP_SUCCESS;

    /* Get pointers */
    pSwipAttrs = pCfg->attrs;
    
    /* Get PRU GPIO Output Control register address */
    if (pSwipAttrs->pruInstId == PRUICSS_PRU0)
    {
        pPruGpioOutCtrlReg = (volatile uint32_t *)(CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_ICSSM_PRU0_GPIO_OUT_CTRL);
    }
    else if (pSwipAttrs->pruInstId == PRUICSS_PRU1)
    {
        pPruGpioOutCtrlReg = (volatile uint32_t *)(CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_ICSSM_PRU1_GPIO_OUT_CTRL);
    }
    else
    {
        status = SystemP_FAILURE;
    }
    
    if (status == SystemP_SUCCESS)
    {
        
        regVal = HW_RD_REG32(pPruGpioOutCtrlReg);
        
        /* Configure PRU GPIO inputs */
        mask = 0;
        mask |= 1 << pSwipAttrs->bclkPin.pinNum;
        mask |= 1 << pSwipAttrs->fsyncPin.pinNum;
        for (i = 0; i < pSwipAttrs->numRxI2s; i++)
        {
            mask |= 1 << pSwipAttrs->rxPin[i].pinNum;
        }
        regVal |= mask;

        /* Configure PRU GPO outputs */
        mask = 0;
        for (i = 0; i < pSwipAttrs->numTxI2s; i++)
        {
            mask |= 1 << pSwipAttrs->txPin[i].pinNum;
        }
        regVal &= ~mask;

        HW_WR_REG32(pPruGpioOutCtrlReg, regVal);
    }
    
    return status;
}

/* De-initializes PRU for PRU I2S */
static int32_t PRUI2S_deinitPru(
    PRUI2S_Config *pCfg
)
{
    PRUI2S_Object *pObj;
    PRUI2S_SwipAttrs *pSwipAttrs;
    PRUICSS_Handle pruIcssHandle;
    uint8_t pruInstId;
    int32_t size;
    int32_t status = SystemP_SUCCESS;   

    /* Get pointers */
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;
    
    pruIcssHandle = pObj->pruIcssHandle;
    pruInstId = pSwipAttrs->pruInstId;

    /* Disable PRU core */
    status = PRUICSS_disableCore(pruIcssHandle, pruInstId);
    
    if (status == SystemP_SUCCESS)
    {    
        /* Reset PRU core */
        status = PRUICSS_resetCore(pruIcssHandle, pruInstId);
    }

    if (status == SystemP_SUCCESS)
    {    
        /* Initialize DMEM to 0 */
        size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_DATARAM(pruInstId));
        if (size == 0)
        {
            status = SystemP_FAILURE;
        }
    }
    
    if (status == SystemP_SUCCESS)
    {    
        /* Initialize IMEM to 0 */
        size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_IRAM_PRU(pruInstId));
        if (size == 0)
        {
            status = SystemP_FAILURE;
        }
        else 
        {
            pObj->pruIcssHandle = NULL;
        }
    }

    return status;
}

/* Unlock pinmux -- copied from pinmux.c */
static void Pinmux_lockMMR(uint32_t domainId)
{
    uint32_t            baseAddr;
    volatile uint32_t  *kickAddr;

    /*Lock IOMUX*/
    baseAddr = (uint32_t) CSL_IOMUX_U_BASE;
    kickAddr = (volatile uint32_t *) (baseAddr + CSL_IOMUX_IO_CFG_KICK0);
    CSL_REG32_WR(kickAddr, IOMUX_KICK_LOCK_VAL);      /* KICK 0 */
    kickAddr = (volatile uint32_t *) (baseAddr + CSL_IOMUX_IO_CFG_KICK1);
    CSL_REG32_WR(kickAddr, IOMUX_KICK_LOCK_VAL);      /* KICK 1 */

    return;
}

/* Lock pinmux -- copied from pinmux.c */
static void Pinmux_unlockMMR(uint32_t domainId)
{
    uint32_t            baseAddr;
    volatile uint32_t  *kickAddr;

    /*Unlock IOMUX*/
    baseAddr = (uint32_t) CSL_IOMUX_U_BASE;
    kickAddr = (volatile uint32_t *) (baseAddr + CSL_IOMUX_IO_CFG_KICK0);
    CSL_REG32_WR(kickAddr, IOMUX_KICK0_UNLOCK_VAL);      /* KICK 0 */
    kickAddr = (volatile uint32_t *) (baseAddr + CSL_IOMUX_IO_CFG_KICK1);
    CSL_REG32_WR(kickAddr, IOMUX_KICK1_UNLOCK_VAL);      /* KICK 1 */

    return;
}



int32_t PRUICSS_enableEvent(PRUICSS_Handle handle, uint32_t eventnum)
{
    uintptr_t               baseaddr;
    PRUICSS_HwAttrs const   *hwAttrs;
    int32_t                 retVal = SystemP_FAILURE;

    if (handle != NULL)
    {
        hwAttrs = (PRUICSS_HwAttrs const *)handle->hwAttrs;
        baseaddr = hwAttrs->intcRegBase;
        
        HW_WR_FIELD32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_SET_INDEX_REG), CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_SET_INDEX_REG_ENABLE_SET_INDEX, eventnum);
        retVal = SystemP_SUCCESS;
    }
    return retVal;
}

int32_t PRUICSS_disableEvent(PRUICSS_Handle handle, uint32_t eventnum)
{
    uintptr_t               baseaddr;
    PRUICSS_HwAttrs const   *hwAttrs;
    int32_t                 retVal = SystemP_FAILURE;

    if (handle != NULL)
    {
        hwAttrs = (PRUICSS_HwAttrs const *)handle->hwAttrs;
        baseaddr = hwAttrs->intcRegBase;
        
        HW_WR_FIELD32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_CLR_INDEX_REG), CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_CLR_INDEX_REG_ENABLE_CLR_INDEX, eventnum);
        retVal = SystemP_SUCCESS;
    }
    return retVal;
}

int32_t PRUICSS_intcCfg(
    PRUICSS_Handle handle,
    const PRUICSS_IntcInitData *intcInitData
)
{
    uintptr_t               baseaddr;
    PRUICSS_HwAttrs const   *hwAttrs;
    PRUICSS_Object          *object;
    uint32_t                i = 0;
    uint32_t                mask[5];
    int32_t                 retVal = SystemP_SUCCESS;
    uint32_t                regVal;

    if((handle == NULL) || (intcInitData == NULL))
    {
        retVal = SystemP_FAILURE;
    }

    if(retVal == SystemP_SUCCESS)
    {
        object = (PRUICSS_Object *)handle->object;
        if (object == NULL)
        {
            retVal = SystemP_FAILURE;
        }
        hwAttrs = (PRUICSS_HwAttrs const *)handle->hwAttrs;
        baseaddr = hwAttrs->intcRegBase;

        for (i = 0;
             ((intcInitData->sysevtToChannelMap[i].sysevt != 0xFF)
             && (intcInitData->sysevtToChannelMap[i].channel != 0xFF));
             i++)
        {
            PRUICSS_intcSetCmr(intcInitData->sysevtToChannelMap[i].sysevt,
                               intcInitData->sysevtToChannelMap[i].channel,
                               ((uint8_t)(~(intcInitData->sysevtToChannelMap[i].polarity)))&0x01U,
                               ((uint8_t)(~(intcInitData->sysevtToChannelMap[i].type)))&0x01U,
                               baseaddr);
        }

        for (i = 0;
               ((i<PRUICSS_INTC_NUM_HOST_INTERRUPTS) &&
               (intcInitData->channelToHostMap[i].channel != 0xFF) &&
               (intcInitData->channelToHostMap[i].host != 0xFF));
               i++)
        {
            PRUICSS_intcSetHmr(intcInitData->channelToHostMap[i].channel,
                               intcInitData->channelToHostMap[i].host,
                               baseaddr);
        }

        mask[0] = 0;
        mask[1] = 0;
        mask[2] = 0;
        mask[3] = 0;
        mask[4] = 0;

        for (i = 0; (uint8_t)intcInitData->sysevtsEnabled[i] != 0xFFU; i++)
        {
            if (intcInitData->sysevtsEnabled[i] < 32)
            {
                mask[0] = mask[0] + (((uint32_t)1U) << (intcInitData->sysevtsEnabled[i]));
            }
            else if (intcInitData->sysevtsEnabled[i] < 64)
            {
                mask[1] = mask[1] + (((uint32_t)1U) << (intcInitData->sysevtsEnabled[i] - 32));
            }
            else if (intcInitData->sysevtsEnabled[i] < 96)
            {
                mask[2] = mask[2] + (((uint32_t)1U) << (intcInitData->sysevtsEnabled[i] - 64));
            }
            else if (intcInitData->sysevtsEnabled[i] < 128)
            {
                mask[3] = mask[3] + (((uint32_t)1U) << (intcInitData->sysevtsEnabled[i] - 96));
            }
            else if (intcInitData->sysevtsEnabled[i] < 160)
            {
                mask[4] = mask[4] + (((uint32_t)1U) << (intcInitData->sysevtsEnabled[i] - 128));
            }
            else
            {
                /* Error */
                retVal = SystemP_FAILURE;
                break;
            }
        }
    }

    if(retVal == SystemP_SUCCESS)
    {
        regVal = HW_RD_REG32(baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_REG0);
        regVal |= mask[0];
        HW_WR_REG32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_REG0), regVal);
        regVal = HW_RD_REG32(baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENA_STATUS_REG0);
        regVal |= mask[0];
        HW_WR_REG32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENA_STATUS_REG0), regVal);

        regVal = HW_RD_REG32(baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_REG1);
        regVal |= mask[1];
        HW_WR_REG32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_REG1), regVal);
        regVal = HW_RD_REG32(baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENA_STATUS_REG1);
        regVal |= mask[1];
        HW_WR_REG32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENA_STATUS_REG1), regVal);

        regVal = HW_RD_REG32(baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_HINT_REG0);
        regVal |= intcInitData->hostEnableBitmask;
        HW_WR_REG32((baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_ENABLE_HINT_REG0), regVal);
    }
    return retVal;
}


static void PRUICSS_intcSetCmr(uint8_t sysevt,
                               uint8_t channel,
                               uint8_t polarity,
                               uint8_t type,
                               uintptr_t baseaddr)
{
    uintptr_t tempAddr1 = 0U;
    uintptr_t tempAddr2 = 0U;

    tempAddr1 = ((baseaddr)+(CSL_ICSS_M_PR1_ICSS_INTC_SLV_CH_MAP_REG0 + (((uint32_t)sysevt) & ~((uint32_t)0x3U))));
    CSL_REG32_WR(tempAddr1, CSL_REG32_RD(tempAddr1) | ((((uint32_t)channel) & ((uint32_t)0xFU)) << ((((uint32_t)sysevt) & ((uint32_t)0x3U)) << 3U)));

    /* TODO: Review the following code*/
    switch ((sysevt >> 5))
    {
    case 0:
        tempAddr1 = (baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_POLARITY_REG0);
        tempAddr2 = (baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_TYPE_REG0);
        CSL_REG32_WR(tempAddr1, CSL_REG32_RD (tempAddr1) & ~(((uint32_t)polarity) << sysevt));
        CSL_REG32_WR(tempAddr2, CSL_REG32_RD (tempAddr2) & ~(((uint32_t)type) << sysevt));
        break;
    case 1:
        tempAddr1 = (baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_POLARITY_REG1);
        tempAddr2 = (baseaddr + CSL_ICSS_M_PR1_ICSS_INTC_SLV_TYPE_REG1);
        CSL_REG32_WR(tempAddr1, CSL_REG32_RD (tempAddr1) & ~(((uint32_t)polarity) << (sysevt - 32U)));
        CSL_REG32_WR(tempAddr2, CSL_REG32_RD (tempAddr2) & ~(((uint32_t)type) << (sysevt - 32U)));
        break;
    default:
        break;
    }
}

static void PRUICSS_intcSetHmr(uint8_t channel,
                               uint8_t host,
                               uintptr_t baseaddr)
{
    uintptr_t tempAddr1 = 0U;

    tempAddr1 = ((baseaddr) + (CSL_ICSS_M_PR1_ICSS_INTC_SLV_HINT_MAP_REG0 + (((uint32_t)channel) & ~((uint32_t)0x3U))));
    CSL_REG32_WR(tempAddr1, CSL_REG32_RD(tempAddr1) | ((((uint32_t)host) & ((uint32_t)0xFU)) << ((((uint32_t)channel) & ((uint32_t)0x3U)) << 3U)));
}

int32_t PRUICSS_intcSetSysEvtChMap(
    PRUICSS_Handle handle, 
    uint8_t sysevt,
    uint8_t channel
)
{
    uintptr_t               baseaddr;
    PRUICSS_HwAttrs const   *hwAttrs;
    int32_t                 retVal = SystemP_FAILURE;
    uintptr_t tempAddr1 = 0U;

    if (handle != NULL)
    {
        hwAttrs = (PRUICSS_HwAttrs const *)handle->hwAttrs;
        baseaddr = hwAttrs->intcRegBase;

        tempAddr1 = ((baseaddr)+(CSL_ICSS_M_PR1_ICSS_INTC_SLV_CH_MAP_REG0 + (((uint32_t)sysevt) & ~((uint32_t)0x3U))));
        CSL_REG32_WR(tempAddr1, CSL_REG32_RD(tempAddr1) | ((((uint32_t)channel) & ((uint32_t)0xFU)) << ((((uint32_t)sysevt) & ((uint32_t)0x3U)) << 3U)));
        retVal = SystemP_SUCCESS;
    }
    return retVal;
}
