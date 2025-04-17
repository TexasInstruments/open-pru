/*
 * Copyright (C) 2025 Texas Instruments Incorporated
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

#ifndef PRU_I2S_DRV_H_
#define PRU_I2S_DRV_H_

#include <stdint.h>
#include <drivers/hw_include/cslr.h>
#include <string.h>
#include <drivers/hw_include/tistdtypes.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/pruicss.h>
#include <drivers/pruicss/m_v0/pruicss.h>
#include <drivers/pruicss/m_v0/cslr_icss_m.h>
#include <drivers/pinmux.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <firmware/TDM4/icss_pru_i2s_fw.h>
/* Number of ICSS instances */
#define PRUI2S_NUM_ICSS_INST            ( 2 )
/* Number of PRU instances per ICSS */
#define PRUI2S_NUM_PRU_INST_PER_ICSS    ( 2 )

/* Maximum number of PRU I2S instances */
#define PRU_I2S_MAX_NUM_INST            ( PRUI2S_NUM_ICSS_INST * PRUI2S_NUM_PRU_INST_PER_ICSS )

/* Default interrupt priorities.
   VIM: 0(highest)..15(lowest). */
#define PRUI2S_VIM_PRI_HIGHEST          ( 0U )
#define PRUI2S_VIM_PRI_LOWEST           ( 15U )
#define DEF_I2S_INTR_PRI                ( PRUI2S_VIM_PRI_HIGHEST )   /* I2S */
#define DEF_I2S_ERR_INTR_PRI            ( PRUI2S_VIM_PRI_HIGHEST )   /* I2S error */

/* Maximum VIM interrupt priority value */
#define MAX_VIM_INTR_PRI_VAL            ( PRUI2S_VIM_PRI_LOWEST )

/* PRU GP Mux Select */
#define PRUICSS_G_MUX_EN                ( PRUICSS_SA_MUX_MODE_DEFAULT ) /* ICSSG_SA_MX_REG:G_MUX_EN */

/* Number INTC system events per PRU */
#define PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU \
    ( 3 )

/* PRU INTC system event indices */
#define PRUI2S_PRU_INTC_SYSEVT0_IDX     ( 0 )  /* I2S Tx system event index */
#define PRUI2S_PRU_INTC_SYSEVT1_IDX     ( 1 )  /* I2S Rx system event index */
#define PRUI2S_PRU_INTC_SYSEVT2_IDX     ( 2 )  /* I2S error system event index */

/* Number INTC channels per PRU */
#define PRUI2S_PRUICSS_INTC_NUM_CHANNELS_PER_PRU \
    ( 3 )

/* PRU-ARM Interrupt Events for INTC system event range (16-31)
   Used for validating system event numbers in INTC configuration */
#define PRU_ARM_EVENT00         16  /* Lowest PRU-ARM event number */
#define PRU_ARM_EVENT15         31  /* Highest PRU-ARM event number */

/* Support number of bits per I2S slot */
#define PRUI2S_BITS_PER_SLOT_16         ( 16 )  /* 16 bits per slot */
#define PRUI2S_BITS_PER_SLOT_32         ( 32 )  /* 32 bits per slot */

#define PRUI2S_MAX_PRU_PIN_NUM          ( 19 )  /* PRU maximum pin number */

/* NOTE: All GPIO and pinmux configuration is now handled by SysConfig.
   No hardcoded PAD or GPIO configuration is needed in the driver. */

/* Return status codes */
#define PRUI2S_DRV_SOK              (  0 )  /* OK */
#define PRUI2S_DRV_SERR_INV_PRMS    ( -1 )  /* Invalid function parameters */
#define PRUI2S_DRV_SERR_INIT        ( -2 )  /* Initialization error */
#define PRUI2S_DRV_SERR_INIT_FWIMG  ( -3 )  /* Initialization error, missing FW image */
#define PRUI2S_DRV_SERR_PRU_EN      ( -4 )  /* PRU core enable error */
#define PRUI2S_DRV_SERR_CLOSE       ( -5 )  /* Close error */
#define PRUI2S_DRV_SERR_CLR_INT     ( -6 )  /* INTC clear event error */

/* Number of PRU I2S configurations */
#define PRU_I2S_NUM_CONFIG  ( 2 )

/* Number of PRU I2S PRU images (firmware images) */
#define PRU_I2S_NUM_PRU_IMAGE   ( 2 )

/* Maximum number of TX I2S */
#define PRUI2S_MAX_TX_I2S   ( 3 )
/* Maximum number of RX I2S */
#define PRUI2S_MAX_RX_I2S   ( 2 )
/* Maximum of TX & RX I2S */
#if (PRUI2S_MAX_TX_I2S < PRUI2S_MAX_RX_I2S)
#define PRUI2S_MAX_I2S       ( PRUI2S_MAX_RX_I2S )
#else
#define PRUI2S_MAX_I2S       ( PRUI2S_MAX_TX_I2S )
#endif

/* Interrupt types */
#define INTR_TYPE_I2S_TX    ( 0 )
#define INTR_TYPE_I2S_RX    ( 1 )
#define INTR_TYPE_I2S_ERR   ( 2 )

/* Error status/mask bit-field bit locations */
#define I2S_ERR_OVR_BN      ( 0 )   /* Rx overflow error bit number */
#define I2S_ERR_UND_BN      ( 1 )   /* Tx underflow error bit number */
#define I2S_ERR_FSYNC_BN    ( 2 )   /* Frame sync error bit number */

/* PRU I2S handle returned by open function */
typedef void *PRUI2S_Handle;

/* Callback function */
typedef void (*PRUI2S_CallbackFxn) (PRUI2S_Handle handle);

/* IO Buffer for Write/Read */
typedef struct PRUI2S_IoBuf_s {
    void *ioBufAddr;                /* Interleaved buffer: Ch0 L, Ch1 L, ..., ChM L, Ch0 R, Ch1 R, ..., ChM R. */
} PRUI2S_IoBuf;

/* Conversion IO Buffer for Write/Read with conversion */
typedef struct PRUI2S_IoBufC_s {
    void *ioBufLAddr[PRUI2S_MAX_I2S];    /* Left channel buffers */
    void *ioBufRAddr[PRUI2S_MAX_I2S];    /* Right channel buffers */
} PRUI2S_IoBufC;
/* PRU I2S FW image info */
typedef struct PRUI2S_PruFwImageInfo_s {
    const uint32_t *pPruImemImg;
    const uint8_t *pPruDmemImg;
    const uint32_t pruImemImgSz;
    const uint32_t pruDmemImgSz;
} PRUI2S_PruFwImageInfo;
/**
 * \brief PRU I2S Parameters Structure
 *
 * Parameters passed to PRUI2S_init() to configure runtime behavior.
 * Use PRUI2S_paramsInit() to initialize with default values, then
 * modify fields as needed before calling PRUI2S_init().
 *
 * Example:
 *   PRUI2S_Params params;
 *   PRUI2S_paramsInit(&params);
 *   params.pruicss_handle = gPruIcssHandle;
 *   params.txPingPongBaseAddr = (uint32_t)txBuffer;
 *   params.i2sTxCallbackFxn = &myTxCallback;
 *   hPruI2s = PRUI2S_init(CONFIG_PRU_I2S0, &params);
 */
typedef struct PRUI2S_Params_s {
    /* PRUICSS handle - must be set before calling PRUI2S_init() */
    PRUICSS_Handle pruicss_handle;

    /* Buffer configuration */
    uint32_t rxPingPongBaseAddr;    /* Rx ping/pong base address */
    uint32_t txPingPongBaseAddr;    /* Tx ping/pong base address */
    uint16_t pingPongBufSz;         /* Tx/Rx ping/pong buffer size (bytes) */

    /* Interrupt priorities (VIM: 0=highest, 15=lowest) */
    uint8_t i2sTxIntrPri;           /* I2S Tx interrupt priority */
    uint8_t i2sRxIntrPri;           /* I2S Rx interrupt priority */
    uint8_t i2sErrIntrPri;          /* I2S error interrupt priority */

    /* Callback functions */
    PRUI2S_CallbackFxn i2sTxCallbackFxn;    /* Callback for I2S Tx complete */
    PRUI2S_CallbackFxn i2sRxCallbackFxn;    /* Callback for I2S Rx complete */
    PRUI2S_CallbackFxn i2sErrCallbackFxn;   /* Callback for I2S errors */
} PRUI2S_Params;
/* PRU I2S object */
typedef struct PRUI2S_Object_s {
    PRUI2S_Params prms;             /* Configured parameters */
    
    void *i2sTxHwiHandle;           /* Interrupt handle for I2S Tx */
    HwiP_Object i2sTxHwiObj;        /* Interrupt object for I2S Tx */
    void *i2sRxHwiHandle;           /* Interrupt handle for I2S Rx */
    HwiP_Object i2sRxHwiObj;        /* Interrupt object for I2S Rx */
    void *i2sErrHwiHandle;          /* Interrupt handle I2S error */
    HwiP_Object i2sErrHwiObj;       /* Interrupt object for I2S error */    
    
    Bool isOpen;                    /* Flag to indicate whether the instance is opened already */    

    /* PRU access (r/w) lock */
    SemaphoreP_Object pruInstlockObj;
    void *pruInstLock;
    
    PRUICSS_Handle pruIcssHandle;   /* PRU ICSS instance handle */
    
    /* PRUI2S FW image info */
    PRUI2S_PruFwImageInfo *pPruFwImageInfo;
    
    void *rxPingPongBuf;            /* Rx ping/pong buffer address */
    void *txPingPongBuf;            /* Tx ping/pong buffer address */
} PRUI2S_Object;

/**
 * \brief PRU I2S SW IP Attributes (Legacy - Deprecated)
 *
 * DEPRECATED: This structure is being replaced by PRUI2S_Attrs.
 * Kept for backward compatibility during transition.
 * New code should use PRUI2S_Attrs and SysConfig-generated configuration.
 *
 * This structure mixes runtime-detected and manually-configured values,
 * which is not the recommended pattern. Use PRUI2S_Attrs instead.
 */
typedef struct PRUI2S_SwipAttrs_s {
    uint32_t baseAddr;              /* Base address (calculated from ICSS instance) */
    uint8_t icssInstId;             /* ICSS instance ID (0=ICSSM0, 1=ICSSM1) - from manual config */
    uint8_t pruInstId;              /* PRU core ID (0=PRU0, 1=PRU1) - from manual config */
    uint8_t numTxI2s;               /* Number of Tx I2S (detected from firmware at runtime) */
    uint8_t numRxI2s;               /* Number of Rx I2S (detected from firmware at runtime) */
    uint8_t sampFreq;               /* Sampling frequency in kHz (detected from firmware at runtime) */
    uint8_t bitsPerSlot;            /* Number of bits per I2S slot (detected from firmware at runtime) */

    /* Host interrupt numbers (detected from firmware at runtime) */
    uint32_t i2sTxHostIntNum;           /* I2S Tx host interrupt number */
    uint32_t i2sRxHostIntNum;           /* I2S Rx host interrupt number */
    uint32_t i2sErrHostIntNum;          /* I2S error host interrupt number */

    /* INTC system event numbers (detected from firmware at runtime) */
    uint8_t i2sTxIcssIntcSysEvt;        /* I2S Tx INTC system event */
    uint8_t i2sRxIcssIntcSysEvt;        /* I2S Rx INTC system event */
    uint8_t i2sErrIcssIntcSysEvt;       /* I2S error INTC system event */

    /* Application calls PRUICSS_intcInit() directly before PRUI2S_init() */
} PRUI2S_SwipAttrs;


/* PRU I2S configuration struture */
typedef struct PRUI2S_Config_s {
    /* Pointer to PRU I2S object */
    PRUI2S_Object *object;
    /* Pointer to PRU I2S SW IP attributes */
    PRUI2S_SwipAttrs *attrs;
} PRUI2S_Config;



/* Initialization of parameters */

/* Used to check status and initialization */
static Bool gPruI2sDrvInit = FALSE;

/* Number of valid configurations */
static uint8_t gPruI2sDrvNumValidCfg = 0;

/* PRU I2S objects */
static PRUI2S_Object gPruI2sObject[PRU_I2S_MAX_NUM_INST];

/* PRU I2S SW IP attributes - Minimal configuration
 * All INTC, GPIO, and pinmux now managed by SysConfig.
 * Only essential base configuration remains here.
 */
/* NOTE: This array is initialized with default values.
 * Applications must call PRUI2S_setUserConfig() to configure ICSS instance and PRU core
 * before calling PRUI2S_init().
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


/* INTC initialization is application responsibility via PRUICSS_intcInit() */

//#define _DBG_TX_PP_CAP
#ifdef _DBG_TX_PP_CAP
/* debug */
#define PRUI2S_PP_CAP_BUF_SZ            ( 360U ) /* Debug capture buffer entries */
#define TX_PP_CAP_BUF_SZ ( PRUI2S_PP_CAP_BUF_SZ )
__attribute__((section(".TxPpBufCap"))) uint16_t gTxPpCapBufIdx=0;
__attribute__((section(".TxPpBufCap"))) uint8_t gTxPpSelCapBuf[TX_PP_CAP_BUF_SZ];
__attribute__((section(".TxPpBufCap"))) uint32_t gTxPpSrcAddrCapBuf[TX_PP_CAP_BUF_SZ];
__attribute__((section(".TxPpBufCap"))) uint32_t gTxPpDstAddrCapBuf[TX_PP_CAP_BUF_SZ];
#endif

//#define _DBG_RX_PP_CAP
#ifdef _DBG_RX_PP_CAP
/* debug */
#define RX_PP_CAP_BUF_SZ ( PRUI2S_PP_CAP_BUF_SZ )
__attribute__((section(".RxPpBufCap"))) uint16_t gRxPpCapBufIdx=0;
__attribute__((section(".RxPpBufCap"))) uint8_t gRxPpSelCapBuf[RX_PP_CAP_BUF_SZ];
__attribute__((section(".RxPpBufCap"))) uint32_t gRxPpSrcAddrCapBuf[RX_PP_CAP_BUF_SZ];
__attribute__((section(".RxPpBufCap"))) uint32_t gRxPpDstAddrCapBuf[RX_PP_CAP_BUF_SZ];
#endif

/* ========================================================================== */
/*                          Legacy APIs (DEPRECATED)                          */
/* ========================================================================== */

/**
 * \brief Initialize PRU I2S driver
 *
 * This function reads configuration from firmware at runtime and initializes
 * the driver with detected parameters.
 *
 * IMPORTANT: Application must call PRUICSS_intcInit() BEFORE calling this function.
 *
 * \param pNumValidCfg      Output: Number of valid configurations
 * \param gPruFwImageInfo   Pointer to PRU firmware image information
 *
 * \return Status code (PRUI2S_DRV_SOK on success)
 */
int32_t PRUI2S_init(
    uint8_t *pNumValidCfg, PRUI2S_PruFwImageInfo (*gPruFwImageInfo)[2]
);

/**
 * \brief De-initialize PRU I2S driver
 *
 * \return None
 */
void PRUI2S_deinit(void);

/**
 * \brief Set user configuration for PRU I2S instance
 *
 * This function must be called before PRUI2S_init() to configure the ICSS instance
 * and PRU core for each configuration. It calculates the appropriate base address
 * based on the ICSS instance and PRU core selection.
 *
 * \param configIdx     Configuration index (0 for first PRU, 1 for second PRU)
 * \param icssInstId    ICSS instance ID (0=ICSSM0, 1=ICSSM1)
 * \param pruInstId     PRU core ID (PRUICSS_PRU0 or PRUICSS_PRU1)
 *
 * \return 0 on success, negative error code on failure
 */
int32_t PRUI2S_setUserConfig(
    uint8_t configIdx,
    uint8_t icssInstId,
    uint8_t pruInstId
);

/**
 * \brief Apply PRU I2S application configuration
 *
 * This function applies the user configuration to the PRU I2S driver.
 * It must be called before PRUI2S_init().
 *
 * \return 0 on success, negative error code on failure
 */
int32_t PRUI2S_applyAppConfig(void);

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
);

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
);

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
 *  return: status code
 */
PRUI2S_Handle PRUI2S_open(
    uint32_t index, 
    PRUI2S_Params *pPrms
);

/* Closes PRU I2S instance:
 *  - Disables PRU
 *  - Resets PRU
 *  - Clears associated ICSS IMEM/DMEM.
 *  - Destructs interrupts.
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *
 *  return: status code
*/
int32_t PRUI2S_close(
    PRUI2S_Handle handle
);

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
);

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
);

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
);
   
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
);

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
);

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
);
   
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
);

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
);

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
);

/* 
 *  Disables PRUI2S interrupt
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      intrType    Type of interrupt to disable, I2S Tx/Rx or I2S error
 *
 *  return: status code
 */
int32_t PRUI2S_disableInt(
    PRUI2S_Handle handle, 
    uint8_t intrType
);

/* 
 *  Enables PRUI2S interrupt 
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      intrType    Type of interrupt to disable, I2S Tx/Rx or I2S error
 *
 *  return: status code
 */
int32_t PRUI2S_enableInt(
    PRUI2S_Handle handle, 
    uint8_t intrType
);

/* 
 *  Clears PRUI2S interrupt 
 *
 *  Parameters:
 *      handle      PRUI2S_Handle returned from PRUI2S_open()
 *      intrType    Type of interrupt to disable, I2S Tx/Rx or I2S error
 *
 *  return: status code
 */
int32_t PRUI2S_clearInt(
    PRUI2S_Handle handle, 
    uint8_t intrType
);

/**
 * \brief   This function enables an INTC event. It should be called only after
 *          successful execution of #PRUICSS_intcInit().
 *
 * \param   handle     #PRUICSS_Handle returned from #PRUICSS_open()
 * \param   eventnum   The INTC Event number
 *
 * \return  #SystemP_SUCCESS in case of successful transition, #SystemP_FAILURE
 *          otherwise
 */
int32_t PRUICSS_enableEvent(
    PRUICSS_Handle handle, 
    uint32_t eventnum
);

/**
 * \brief   This function disables an INTC event. It should be called only after
 *          successful execution of #PRUICSS_intcInit().
 *
 * \param   handle     #PRUICSS_Handle returned from #PRUICSS_open()
 * \param   eventnum   The INTC Event number
 *
 * \return  #SystemP_SUCCESS in case of successful transition, #SystemP_FAILURE
 *          otherwise
 */
int32_t PRUICSS_disableEvent(
    PRUICSS_Handle handle, 
    uint32_t eventnum
);

/* Application calls PRUICSS_intcInit() directly for initialization.
 * Runtime control uses PRUICSS_enableEvent() and PRUICSS_disableEvent().
 */

/**
 * \brief    Extracts information in PRU FW pseudo-registers, update SW IP using extracted info.
 *           Info is contained in PRU FW DMEM image.
 *
 * \param    pPruFwImageInfo    Pointer to PRU FW image info structure
 * \param    pSwipAttrs         Pointer to SW IP attributes structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_getPruFwImageInfo(
    PRUI2S_PruFwImageInfo *pPruFwImageInfo, 
    PRUI2S_SwipAttrs *pSwipAttrs
);

/**
 * \brief    Checks SW IP parameters
 *
 * \param    pSwipAttrs         Pointer to SW IP attributes structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_checkSwipParams(
    PRUI2S_SwipAttrs *pSwipAttrs
);

/**
 * \brief    Checks parameters used for PRUI2S_open() function
 *
 * \param    pSwipAttrs         Pointer to SW IP attributes structure
 * \param    pPrms              Pointer to PRU I2S parameters structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_checkOpenParams(
    PRUI2S_SwipAttrs *pSwipAttrs,
    PRUI2S_Params *pPrms
);

/**
 * \brief    Initializes PRU for PRU I2S
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_initPru(
    PRUI2S_Config *pCfg
);

/* INTC initialization is application responsibility via PRUICSS_intcInit() */

/**
 * \brief    Initializes PRU I2S FW
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_initFw(
    PRUI2S_Config *pCfg
);

/**
 * \brief    Initializes PRU I2S ping/pong buffers
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_initPpBufs(
    PRUI2S_Config *pCfg
);

/**
 * \brief    De-initializes PRU for PRU I2S
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_deinitPru(
    PRUI2S_Config *pCfg
);

#endif /* PRU_I2S_DRV_H_ */
