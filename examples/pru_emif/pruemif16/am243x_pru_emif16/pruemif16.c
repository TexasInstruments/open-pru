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

#include <stdint.h>
#include <drivers/hw_include/csl_types.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/pruicss.h>
#include "pruemif16.h"
#include "firmware/include/icssg_pruemif16_fw.h"

/* PRU firmware image arrays */
#include "pruemif16_emif_addr_pru0_bin.h"
#include "pruemif16_emif_data_pru1_bin.h"

#define ICSSG_NUM_SLICE         ( 2 )   /* ICSSG number of slices */

/* Number of PRU images */
#define PRUEMIF16_NUM_PRU_IMAGE ( 2 )

/* PRU FW image info */
typedef struct PruFwImageInfo_s {
    const uint32_t *pPruImemImg;
    const uint32_t pruImemImgSz;
    const uint32_t pruImemImgOffset;
    const uint8_t *pPruDmemImg;
    const uint32_t pruDmemImgSz;
    const uint32_t pruDmemImgOffset;
} PruFwImageInfo;

/* PRU image info */
static PruFwImageInfo gPruFwImageInfo[PRUEMIF16_NUM_PRU_IMAGE] =
{
    {pruemif16_emif_addr_pru0_0, sizeof(pruemif16_emif_addr_pru0_0), 0, NULL, 0, 0}, /* PRU0 FW */
    {pruemif16_emif_data_pru1_0, sizeof(pruemif16_emif_data_pru1_0), 0, NULL, 0, 0}, /* PRU1 FW */
};

int32_t pruEmif16_InitIcss(
    uint8_t icssInstId,             /* Sysconfig ICSS instance */
    PRUICSS_Handle *pPruIcssHandle  /* Address of PRUICSS driver handle */
)
{
    PRUICSS_Handle pruIcssHandle;
    int32_t size;
#if 0 // FL: PRU0
    const PRUICSS_HwAttrs *pPruIcssHwAttrs;
    uint8_t reg16bVal;
#endif
    int32_t status;
    
    /* Open ICSS PRU instance */
    pruIcssHandle = PRUICSS_open(icssInstId);
    if (pruIcssHandle == NULL) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }

    /* Disable PRU cores */
    status = PRUICSS_disableCore(pruIcssHandle, PRUICSS_PRU0);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    status = PRUICSS_disableCore(pruIcssHandle, PRUICSS_PRU1);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }

    /* Reset ICSS memories */
    /* PRU0 IRAM */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_IRAM_PRU(0));
    if (size == 0)
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    /* PRU1 IRAM */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_IRAM_PRU(1));
    if (size == 0) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    /* DMEM0 */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_DATARAM(0));
    if (size == 0) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    /* DMEM1 */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_DATARAM(1));
    if (size == 0) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    /* SHMEM */
    size = PRUICSS_initMemory(pruIcssHandle, PRUICSS_SHARED_RAM);
    if (size == 0) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    
    /* Configure ICSS pin mux */
    status = PRUICSS_setGpMuxSelect(pruIcssHandle, PRUICSS_PRU0, PRUICSS_GP_MUX_SEL_MODE_GP);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    status = PRUICSS_setGpMuxSelect(pruIcssHandle, PRUICSS_PRU1, PRUICSS_GP_MUX_SEL_MODE_GP);
    if (status != SystemP_SUCCESS)
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    status = PRUICSS_setSaMuxMode(pruIcssHandle, PRUICSS_SA_MUX_MODE_DEFAULT);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    
    /* Configure ICSS GPI mode */
    status = PRUICSS_setGpiMode(pruIcssHandle, PRUICSS_PRU0, PRUICSS_GPI_MODE_DIRECT_R31_STATUS);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }
    status = PRUICSS_setGpiMode(pruIcssHandle, PRUICSS_PRU1, PRUICSS_GPI_MODE_DIRECT_R31_STATUS);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_ICSSG;
    }

#if 0 // FL: PRU0
    /* Get ICSSG instance ID hardware attributes */
    pPruIcssHwAttrs = PRUICSS_getAttrs(icssInstId);

    /* Set shared EMIF address to 0 */
    reg16bVal = HW_RD_REG16(pPruIcssHwAttrs->sharedDramBase + FW_REG_EMIF_ADDR);
    reg16bVal &= ~EMIF_ADDR_MASK;
    reg16bVal |= EMIF_ADDR_RESET_VAL;
    HW_WR_REG16(pPruIcssHwAttrs->sharedDramBase + FW_REG_EMIF_ADDR, reg16bVal);
#endif

    *pPruIcssHandle = pruIcssHandle;

    return PRUEMIF16_ERR_NERR;
}

int32_t pruEmif16_initPruIcss(
    PRUICSS_Handle pruIcssHandle,   /* PRUICSS driver handle */
    uint8_t icssInstId,             /* Sysconfig ICSS instance */
    uint8_t pruInstId               /* PRU instance ID */
)
{
    uint8_t sliceId;
    PruFwImageInfo *pPruFwImageInfo;
    uint32_t pruMem;            /* PRU memory address */
    const uint32_t *sourceMem;  /* Source memory[ Array of uint32_t ] */
    uint32_t byteLen;           /* Total number of bytes to be written */
    uint32_t memOffset;         /* Offset at which write will happen */
    int32_t size;
    const PRUICSS_HwAttrs *pPruIcssHwAttrs;
    uintptr_t pruDramBase;
    uint8_t reg8bVal;
    int32_t status;

    /* Reset PRU */
    status = PRUICSS_resetCore(pruIcssHandle, pruInstId);
    if (status != SystemP_SUCCESS) 
    {
        return PRUEMIF16_ERR_INIT_PRU;
    }

    /* Determine PRU FW image */
    pPruFwImageInfo = &gPruFwImageInfo[pruInstId];
    if (pPruFwImageInfo == NULL)
    {
        return PRUEMIF16_ERR_INIT_PRU;
    }

    /* Calculate slice ID */
    sliceId = pruInstId - (uint8_t)pruInstId/ICSSG_NUM_SLICE * ICSSG_NUM_SLICE;

    if (pPruFwImageInfo->pPruDmemImg != NULL)
    {
        /* Determine DMEM address */
        pruMem = PRUICSS_DATARAM(sliceId);
        /* Get DMEM image source address*/
        sourceMem = (uint32_t *)pPruFwImageInfo->pPruDmemImg;
        /* Get DMEM image size */
        byteLen = pPruFwImageInfo->pruDmemImgSz;
        /* Get DMEM image offset */
        memOffset = pPruFwImageInfo->pruDmemImgOffset;
        
        /* Write DMEM */
        size = PRUICSS_writeMemory(pruIcssHandle, pruMem, memOffset, sourceMem, byteLen);
        if (size == 0)
        {
            return PRUEMIF16_ERR_INIT_PRU;
        }
    }
    
    if (pPruFwImageInfo->pPruImemImg != NULL)
    {
        /* Determine IMEM addresses */
        pruMem = PRUICSS_IRAM_PRU(sliceId);
        /* Get IMEM image source address*/
        sourceMem = (uint32_t *)pPruFwImageInfo->pPruImemImg;
        /* Get IMEM image size */
        byteLen = pPruFwImageInfo->pruImemImgSz;
        /* Get IMEM image offset */
        memOffset = pPruFwImageInfo->pruImemImgOffset;
        
        /* Write IMEM */
        size = PRUICSS_writeMemory(pruIcssHandle, pruMem, memOffset, sourceMem, byteLen);
        if (size == 0)
        {
            return PRUEMIF16_ERR_INIT_PRU;
        }
    }
    else
    {
        return PRUEMIF16_ERR_INIT_PRU;
    }
    
    /* Get ICSSG instance ID hardware attributes */
    pPruIcssHwAttrs = PRUICSS_getAttrs(icssInstId);
    
    pruDramBase = (pruInstId == PRUICSS_PRU0) ? pPruIcssHwAttrs->pru0DramBase : pPruIcssHwAttrs->pru1DramBase;
    {
        /* Reset PRU firmware status register */

        reg8bVal = HW_RD_REG8(pruDramBase + FW_REG_FW_STAT);
        /* Reset PRU initialization flag */
        reg8bVal &= ~PRU_INIT_MASK;
        reg8bVal |= PRU_INIT_RESET_VAL;
        /* Reset PRU sharing error flag */
        reg8bVal &= ~SHARE_ERR_MASK;
        reg8bVal |= SHARE_ERR_RESET_VAL;
        HW_WR_REG8(pruDramBase + FW_REG_FW_STAT, reg8bVal);
    }
    
    return PRUEMIF16_ERR_NERR;
}
