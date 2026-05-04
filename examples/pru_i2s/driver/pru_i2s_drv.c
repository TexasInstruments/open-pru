/**
 *  \file pru_i2s_drv.c
 *
 *  \brief PRU I2S driver implementation
 *
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

#include <stdint.h>
#include "pru_i2s_drv.h"
#include <pru_i2s/firmware/TDM4/icss_pru_i2s_fw.h>

/* Application calls PRUICSS_intcInit() directly (Motor Control SDK pattern) */

/* Used to check status and initialization */
static Bool gPruI2sDrvInit = FALSE;

/* Number of valid configurations */
static uint8_t gPruI2sDrvNumValidCfg = 0;

/* PRU I2S objects */
static PRUI2S_Object gPruI2sObject[PRU_I2S_MAX_NUM_INST];

/* PRU I2S SW IP attributes - Minimal configuration
 * All INTC, GPIO, and pinmux now managed by SysConfig.
 * Only essential base configuration remains here.
 * NOTE: Applications must call PRUI2S_setUserConfig() to configure ICSS instance
 * and PRU core before calling PRUI2S_init().
 */
static PRUI2S_SwipAttrs gPruI2sSwipAttrs[PRU_I2S_MAX_NUM_INST] =
{
    /* Configuration 0 - Set by PRUI2S_setUserConfig() */
    {
        .baseAddr = 0,                      /* Set by PRUI2S_setUserConfig() based on ICSS instance */
        .icssInstId = 0,                    /* Set by PRUI2S_setUserConfig() */
        .pruInstId = PRUICSS_PRU0,          /* Set by PRUI2S_setUserConfig() */
        .numTxI2s = 0,                      /* Detected from firmware at runtime */
        .numRxI2s = 0,                      /* Detected from firmware at runtime */
        .sampFreq = 0,                      /* Detected from firmware at runtime */
        .bitsPerSlot = 0,                   /* Detected from firmware at runtime */
        .i2sTxHostIntNum = 0,               /* Detected from firmware at runtime */
        .i2sRxHostIntNum = 0,               /* Detected from firmware at runtime */
        .i2sErrHostIntNum = 0,              /* Detected from firmware at runtime */
        .i2sTxIcssIntcSysEvt = 0,           /* Detected from firmware at runtime */
        .i2sRxIcssIntcSysEvt = 0,           /* Detected from firmware at runtime */
        .i2sErrIcssIntcSysEvt = 0,          /* Detected from firmware at runtime */
    },
    /* Configuration 1 - Set by PRUI2S_setUserConfig() */
    {
        .baseAddr = 0,                      /* Set by PRUI2S_setUserConfig() based on ICSS instance */
        .icssInstId = 0,                    /* Set by PRUI2S_setUserConfig() */
        .pruInstId = PRUICSS_PRU1,          /* Set by PRUI2S_setUserConfig() */
        .numTxI2s = 0,                      /* Detected from firmware at runtime */
        .numRxI2s = 0,                      /* Detected from firmware at runtime */
        .sampFreq = 0,                      /* Detected from firmware at runtime */
        .bitsPerSlot = 0,                   /* Detected from firmware at runtime */
        .i2sTxHostIntNum = 0,               /* Detected from firmware at runtime */
        .i2sRxHostIntNum = 0,               /* Detected from firmware at runtime */
        .i2sErrHostIntNum = 0,              /* Detected from firmware at runtime */
        .i2sTxIcssIntcSysEvt = 0,           /* Detected from firmware at runtime */
        .i2sRxIcssIntcSysEvt = 0,           /* Detected from firmware at runtime */
        .i2sErrIcssIntcSysEvt = 0,          /* Detected from firmware at runtime */
    }
};

/* PRU I2S configurations */
static PRUI2S_Config gPruI2sConfig[PRU_I2S_NUM_CONFIG] =
{
    {
        &gPruI2sObject[0],
        &gPruI2sSwipAttrs[0]
    },
    {
        &gPruI2sObject[1],
        &gPruI2sSwipAttrs[1]
    }
};

/*
 * Sets user configuration for a PRU I2S instance
 *
 * This function configures the ICSS instance and PRU core for a specific
 * configuration index. It calculates the appropriate base address based on
 * the ICSS instance and PRU core selection.
 *
 * Parameters:
 *     configIdx: Configuration index (0 or 1)
 *     icssInstId: ICSS instance ID (0=ICSSM0, 1=ICSSM1)
 *     pruInstId: PRU core ID (PRUICSS_PRU0 or PRUICSS_PRU1)
 *
 * return: status code
 */
int32_t PRUI2S_setUserConfig(
    uint8_t configIdx,
    uint8_t icssInstId,
    uint8_t pruInstId
)
{
    PRUI2S_SwipAttrs *pSwipAttrs;

    /* Validate parameters */
    if (configIdx >= PRU_I2S_NUM_CONFIG)
    {
        return PRUI2S_DRV_SERR_INV_PRMS;
    }

    if (icssInstId >= PRUI2S_NUM_ICSS_INST)
    {
        return PRUI2S_DRV_SERR_INV_PRMS;
    }

    if ((pruInstId != PRUICSS_PRU0) && (pruInstId != PRUICSS_PRU1))
    {
        return PRUI2S_DRV_SERR_INV_PRMS;
    }

    /* Update attributes - baseAddr will be calculated in PRUI2S_open() once PRUICSS handle is available */
    pSwipAttrs = gPruI2sConfig[configIdx].attrs;
    pSwipAttrs->icssInstId = icssInstId;
    pSwipAttrs->pruInstId = pruInstId;
    /* Note: baseAddr is set in PRUI2S_open() using PRUICSS handle hwAttrs */

    return PRUI2S_DRV_SOK;
}

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
    uint8_t successfulInits = 0; /* Track successful initializations for cleanup */


    /* Validate input parameters */
    if (pNumValidCfg == NULL)
    {
        return PRUI2S_DRV_SERR_INV_PRMS;
    }
    if (gPruFwImageInfo == NULL)
    {
        *pNumValidCfg = 0;
        return PRUI2S_DRV_SERR_INV_PRMS;
    }


    if (gPruI2sDrvInit == FALSE)
    {

        /* Reset valid config count at start of initialization */
        gPruI2sDrvNumValidCfg = 0;

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
                successfulInits = i + 1; /* Track successful mutex construction */
            }
            else
            {
                retStatus = PRUI2S_DRV_SERR_INIT;
                /* Cleanup previously constructed mutexes */
                for (uint8_t j = 0; j < i; j++)
                {
                    PRUI2S_Object *pCleanupObj = gPruI2sConfig[j].object;
                    if (pCleanupObj->pruInstLock != NULL)
                    {
                        SemaphoreP_destruct(&pCleanupObj->pruInstlockObj);
                        pCleanupObj->pruInstLock = NULL;
                    }
                }
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
                /* Parse info in PRU image #0, update SW IP info */
                /* Note: baseAddr will be calculated in PRUI2S_open() using PRUICSS handle */
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
                /* Parse info in PRU image #1, update SW IP info */
                /* Note: baseAddr will be calculated in PRUI2S_open() using PRUICSS handle */
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
                /* Cleanup previously constructed mutexes */
                for (uint8_t j = 0; j < successfulInits; j++)
                {
                    PRUI2S_Object *pCleanupObj = gPruI2sConfig[j].object;
                    if (pCleanupObj->pruInstLock != NULL)
                    {
                        SemaphoreP_destruct(&pCleanupObj->pruInstlockObj);
                        pCleanupObj->pruInstLock = NULL;
                    }
                }
                break;
            }

            gPruI2sDrvNumValidCfg++;
        }

        /* Always write output parameter */
        *pNumValidCfg = gPruI2sDrvNumValidCfg;

        if (retStatus == PRUI2S_DRV_SOK)
        {
            gPruI2sDrvInit = TRUE;
        }

    }
    else
    {
        /* Already initialized, populate output parameter */
        *pNumValidCfg = gPruI2sDrvNumValidCfg;
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
            if (pObj->pruInstLock != NULL)
            {
                SemaphoreP_destruct(&pObj->pruInstlockObj);
                pObj->pruInstLock = NULL;
            }

            /* Reset per-object state to safe baseline */
            pObj->pruIcssHandle = NULL;
            pObj->isOpen = FALSE;
            pObj->i2sTxHwiHandle = NULL;
            pObj->i2sRxHwiHandle = NULL;
            pObj->i2sErrHwiHandle = NULL;
        }

        /* Reset driver init state so PRUI2S_init() can be called again */
        gPruI2sDrvNumValidCfg = 0;
        gPruI2sDrvInit = FALSE;
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
    /* PRUICSS handle - must be set by application */
    pPrms->pruicss_handle = NULL;

    /* Buffer configuration */
    pPrms->rxPingPongBaseAddr = 0;
    pPrms->txPingPongBaseAddr = 0;
    pPrms->pingPongBufSz = 0;

    /* Interrupt priorities */
    pPrms->i2sTxIntrPri = DEF_I2S_INTR_PRI;
    pPrms->i2sRxIntrPri = DEF_I2S_INTR_PRI;
    pPrms->i2sErrIntrPri = DEF_I2S_ERR_INTR_PRI;

    /* Callback functions */
    pPrms->i2sTxCallbackFxn = NULL;
    pPrms->i2sRxCallbackFxn = NULL;
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

        /* Store PRUICSS handle in object */
        pObj->pruIcssHandle = pObj->prms.pruicss_handle;

        /* Validate PRUICSS handle */
        if (pObj->pruIcssHandle == NULL)
        {
            status = PRUI2S_DRV_SERR_INV_PRMS;
            goto fail;
        }

        if (status == SystemP_SUCCESS)
        {
            /* Get PRU ICSS hardware attributes */
            PRUICSS_HwAttrs *hwAttrs = (PRUICSS_HwAttrs *)(pObj->pruIcssHandle->hwAttrs);

            if (hwAttrs == NULL)
            {
                status = PRUI2S_DRV_SERR_INV_PRMS;
            }
            else
            {
                /* Calculate base address from PRUICSS handle based on PRU instance */
                uint32_t dmemBaseAddr;

                if (pSwipAttrs->pruInstId == PRUICSS_PRU0)
                {
                    dmemBaseAddr = (uint32_t)(hwAttrs->pru0DramBase);
                }
                else if (pSwipAttrs->pruInstId == PRUICSS_PRU1)
                {
                    dmemBaseAddr = (uint32_t)(hwAttrs->pru1DramBase);
                }
                else
                {
                    status = PRUI2S_DRV_SERR_INV_PRMS;
                }

                if (status == SystemP_SUCCESS)
                {
                    /* Add firmware register base offset */
                    pSwipAttrs->baseAddr = dmemBaseAddr + ICSS_PRUI2S_FW_REG_BASE;

                    /* Translate to local address if needed */
                    pSwipAttrs->baseAddr = (uint32_t)AddrTranslateP_getLocalAddr(pSwipAttrs->baseAddr);
                }
            }
        }

        /* Check parameters are valid */
        if (status == SystemP_SUCCESS)
        {
            status = PRUI2S_checkOpenParams(pSwipAttrs, &pObj->prms);
        }

        if (status != SystemP_SUCCESS)
        {
            goto fail;
        }
    }

    if (status == SystemP_SUCCESS)
    {
        /* Construct interrupts */
        if (pObj->prms.i2sTxCallbackFxn != NULL)
        {
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

        if (status == SystemP_SUCCESS && pObj->prms.i2sRxCallbackFxn != NULL)
        {
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

        if (status == SystemP_SUCCESS && pObj->prms.i2sErrCallbackFxn != NULL)
        {
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
        /* Initialize PRU */
        status = PRUI2S_initPru(pCfg);

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
    }

    if (status == SystemP_SUCCESS)
    {
        pObj->isOpen = TRUE;
        handle = (PRUI2S_Handle)pCfg;
        goto done;
    }

fail:
    /* Destruct any HWIs constructed so far and reset object state */
    if (pObj->i2sTxHwiHandle != NULL)
    {
        HwiP_destruct(&pObj->i2sTxHwiObj);
        pObj->i2sTxHwiHandle = NULL;
    }
    if (pObj->i2sRxHwiHandle != NULL)
    {
        HwiP_destruct(&pObj->i2sRxHwiObj);
        pObj->i2sRxHwiHandle = NULL;
    }
    if (pObj->i2sErrHwiHandle != NULL)
    {
        HwiP_destruct(&pObj->i2sErrHwiObj);
        pObj->i2sErrHwiHandle = NULL;
    }
    /* Reset pruIcssHandle so a future open can retry */
    pObj->pruIcssHandle = NULL;

done:
    /* Unlock instance */
    SemaphoreP_post(pObj->pruInstLock);

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
        /* Disable I2S Tx interrupt - check handle is valid */
        if (pObj->pruIcssHandle != NULL)
        {
            status = PRUICSS_disableEvent(pObj->pruIcssHandle, pSwipAttrs->i2sTxIcssIntcSysEvt);
        }
        else
        {
            status = SystemP_SUCCESS; /* Skip if handle not initialized */
        }
        
        if ((status == SystemP_SUCCESS) && (pObj->pruIcssHandle != NULL))
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
        /* Disable I2S Rx interrupt - check handle is valid */
        if (pObj->pruIcssHandle != NULL)
        {
            status = PRUICSS_disableEvent(pObj->pruIcssHandle, pSwipAttrs->i2sRxIcssIntcSysEvt);
        }
        else
        {
            status = SystemP_SUCCESS; /* Skip if handle not initialized */
        }
        
        if ((status == SystemP_SUCCESS) && (pObj->pruIcssHandle != NULL))
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
        /* Disable I2S error interrupt - check handle is valid */
        if (pObj->pruIcssHandle != NULL)
        {
            status = PRUICSS_disableEvent(pObj->pruIcssHandle, pSwipAttrs->i2sErrIcssIntcSysEvt);
        }
        else
        {
            status = SystemP_SUCCESS; /* Skip if handle not initialized */
        }
        
        /* Clear I2S error pending interrupts */
        if ((status == SystemP_SUCCESS) && (pObj->pruIcssHandle != NULL))
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
        /* Deinitialize PRU - only if handle was initialized */
        if (pObj->pruIcssHandle != NULL)
        {
            status = PRUI2S_deinitPru(pCfg);
        }
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
        /* Validate buffer size is even and non-zero */
        if ((pObj->prms.pingPongBufSz == 0) || ((pObj->prms.pingPongBufSz & 0x1U) != 0U))
        {
            return PRUI2S_DRV_SERR_INV_PRMS;
        }
        dstAddr = (uint8_t *)pObj->txPingPongBuf;
        if (txPingPongSel == PING_PONG_SEL_PONG)
        {
            dstAddr += size;
        }
        /* Ensure destination within allocated range */
        if ((dstAddr < (uint8_t *)pObj->txPingPongBuf) || (dstAddr + size > ((uint8_t *)pObj->txPingPongBuf + pObj->prms.pingPongBufSz)))
        {
            return PRUI2S_DRV_SERR_INV_PRMS;
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
     * NOTE: GPIO pin configuration is now handled by SysConfig.
     * Pin numbers are defined in firmware but GPIO/pinmux is configured via SysConfig.
     * No need to store or validate pin numbers in driver.
     */

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

    pPruIcssHwAttrs = PRUICSS_getAttrs(pSwipAttrs->icssInstId);
    icssHwInstId = pPruIcssHwAttrs->instance;

    /* Note: baseAddr is calculated in PRUI2S_open(), so skip check here during init */
    /* Skipping baseAddr check during PRUI2S_init() */

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
       Note: Host interrupt validation is now device/instance-agnostic.
       Application provides interrupt numbers via SysConfig, which handles validation.
     */
    /* Host interrupt validation is now delegated to SysConfig */
    (void)icssHwInstId;  /* Mark icssHwInstId as used to avoid compiler warnings */

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
    
    /* NOTE: GPIO pin validation is no longer needed.
     * GPIO/pinmux is now configured via SysConfig, which handles pin validation. */
    
    return status;
}

/* Checks parameters used for PRUI2S_open() function */
static int32_t PRUI2S_checkOpenParams(
    PRUI2S_SwipAttrs *pSwipAttrs,
    PRUI2S_Params *pPrms
)
{
    int32_t status = SystemP_SUCCESS;    
    
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
           Tx ping/pong buffer is expected to be in ICSS SHMEM.
           Note: Absolute buffer address validation is delegated to application
           during PRUI2S_open() when full PRUICSS handle context is available. */
        if ((status == SystemP_SUCCESS) && (pPrms->txPingPongBaseAddr == 0))
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
    uint8_t pruInstId;
    PRUICSS_Handle pruIcssHandle;
    int32_t size;
    int32_t status = SystemP_SUCCESS;


    /* Get pointers */
    pObj = pCfg->object;
    pSwipAttrs = pCfg->attrs;
    pPrms = &pCfg->object->prms;


    if (pPrms == NULL)
    {
        return PRUI2S_DRV_SERR_INV_PRMS;
    }

    pruInstId = pSwipAttrs->pruInstId;

    /* Use PRUICSS handle already set in PRUI2S_open() */
    pruIcssHandle = pObj->pruIcssHandle;

    if (pruIcssHandle == NULL)
    {
        return PRUI2S_DRV_SERR_INV_PRMS;
    }

    /* Disable PRU core */
    status = PRUICSS_disableCore(pruIcssHandle, pruInstId);
    if (status != SystemP_SUCCESS)
    {
        return PRUI2S_DRV_SERR_INIT;
    }

    /* Set ICSS pin mux */
    status = PRUICSS_setSaMuxMode(pruIcssHandle, PRUICSS_G_MUX_EN);
    if (status != SystemP_SUCCESS)
    {
        return PRUI2S_DRV_SERR_INIT;
    }

    /* Reset PRU core */
    status = PRUICSS_resetCore(pruIcssHandle, pruInstId);
    if (status != SystemP_SUCCESS)
    {
        return PRUI2S_DRV_SERR_INIT;
    }

    /* Initialize DATARAM */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_DATARAM(pruInstId));
    if (size == 0)
    {
        return PRUI2S_DRV_SERR_INIT;
    }

    /* Initialize IRAM */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_IRAM_PRU(pruInstId));
    if (size == 0)
    {
        return PRUI2S_DRV_SERR_INIT;
    }

    /* Firmware loading is handled by application in prui2s_pruicss_load_run_fw()
     * Driver assumes firmware is already loaded when PRUI2S_initPru() is called
     * This avoids duplicate firmware loads and simplifies driver initialization
     *
     * PRU core enable is also handled by application in prui2s_pruicss_load_run_fw()
     * Driver does not enable cores here to avoid conflicts with application
     */

    return SystemP_SUCCESS;
}

/* Application calls PRUICSS_intcInit() directly before PRUI2S_open().
 * This matches Motor Control SDK pattern (HDSL, EnDAT3, BiSS-C).
 */

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
        /* Rx buffer address passed from application is expected to be the target address
           (either ICSS local address or absolute system address).
           This is now application/SysConfig responsibility to provide correctly.
           Firmware will use this address as-is. */
        tmpRxPpBufAddr = pObj->prms.rxPingPongBaseAddr;

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
        /* Get PRUICSS hardware attributes from the handle */
        pPruIcssHwAttrs = (PRUICSS_HwAttrs *)(pObj->pruIcssHandle->hwAttrs);

        if (pPruIcssHwAttrs != NULL)
        {
            uint32_t pruDramBase = 0;


            /* Get PRU-specific DRAM base address from hardware attributes - device/instance agnostic */
            /* The PRUICSS handle provides the actual DRAM mapping for any PRU on any device */
            if (pSwipAttrs->pruInstId == PRUICSS_PRU0)
            {
                pruDramBase = (uint32_t)pPruIcssHwAttrs->pru0DramBase;
            }
            else if (pSwipAttrs->pruInstId == PRUICSS_PRU1)
            {
                pruDramBase = (uint32_t)pPruIcssHwAttrs->pru1DramBase;
            }
            else
            {
                status = SystemP_FAILURE;
            }

            if ((status == SystemP_SUCCESS) && (pruDramBase != 0))
            {
                /* Calculate Tx buffer address using PRU DRAM base + application offset */
                pObj->txPingPongBuf = (void *)(pruDramBase + pObj->prms.txPingPongBaseAddr);
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
            memset((uint8_t *)pObj->txPingPongBuf, 0, pObj->prms.pingPongBufSz);
        }
    }

    if ((status == SystemP_SUCCESS) && (pSwipAttrs->numRxI2s > 0))
    {

        /* Rx buffer address is provided by application as a complete target address (not an offset) */
        /* This allows application to specify buffers in OCRAM, DDR, or other memory locations */
        if (pObj->prms.rxPingPongBaseAddr != 0)
        {
            pObj->rxPingPongBuf = (void *)pObj->prms.rxPingPongBaseAddr;
        }
        else
        {
            status = SystemP_FAILURE;
        }

        if (status == SystemP_SUCCESS)
        {
            memset((uint8_t *)pObj->rxPingPongBuf, 0, pObj->prms.pingPongBufSz);
        }
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

    /* Early return if handle is NULL (init failed before handle assignment) */
    if (pruIcssHandle == NULL)
    {
        return SystemP_SUCCESS;
    }

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

/* Application calls PRUICSS_intcInit() directly for initialization.
 * Runtime control uses PRUICSS_enableEvent() and PRUICSS_disableEvent() below.
 */
