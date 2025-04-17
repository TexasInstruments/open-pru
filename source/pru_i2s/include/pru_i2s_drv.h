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

#ifndef _PRUICSS_DRV_AUX_H_
#define _PRUICSS_DRV_AUX_H_

#include <stdint.h>
#include <drivers/hw_include/tistdtypes.h>
#include <drivers/pruicss.h>

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

/* PRU I2S pinmux attributes */
typedef struct PRUI2S_PruGpioPadAttrs_s {
    uint8_t pinNum;                 /* PRU Pin Number for FW */
    uint32_t padCfgRegAddr;         /* PAD configuration register */
    uint32_t padCfgRegVal;          /* PAD configuration register value */
} PRUI2S_PruGpioPadAttrs;

/* PRU I2S SW IP attributes */
typedef struct PRUI2S_SwipAttrs_s {
    uint32_t baseAddr;              /* base address */
    uint8_t icssInstId;             /* ICSS instance ID */
    uint8_t pruInstId;              /* PRU core ID */
    uint8_t numTxI2s;               /* number of Tx I2S */
    uint8_t numRxI2s;               /* number of Rx I2S */
    uint8_t sampFreq;               /* sampling frequency (kHz) */
    uint8_t bitsPerSlot;            /* number of bits per I2S slot (left or right) */    
    uint32_t i2sTxHostIntNum;       /* I2S Tx host interrupt number */
    uint32_t i2sRxHostIntNum;       /* I2S Rx host interrupt number */
    uint32_t i2sErrHostIntNum;      /* I2S error host interrupt number */
    uint8_t i2sTxIcssIntcSysEvt;    /* I2S Tx ICSS INTC System Event */
    uint8_t i2sRxIcssIntcSysEvt;    /* I2S Rx ICSS INTC System Event */
    uint8_t i2sErrIcssIntcSysEvt;   /* I2S error ICSS INTC System Event */
    
    /* BCLK pin mux register */
    PRUI2S_PruGpioPadAttrs bclkPin;
    /* FSYNC pin mux register */
    PRUI2S_PruGpioPadAttrs fsyncPin;
    /* Tx pin mux registers */
    PRUI2S_PruGpioPadAttrs txPin[PRUI2S_MAX_TX_I2S];
    /* Rx pin mux registers */
    PRUI2S_PruGpioPadAttrs rxPin[PRUI2S_MAX_RX_I2S];
} PRUI2S_SwipAttrs;

/* PRU I2S parameters */
typedef struct PRUI2S_Params_s {
    Bool pruI2sEn;                  /* PRU I2S enable flag, indicates whether to enable PRU immediately */
    uint32_t rxPingPongBaseAddr;    /* Rx ping/pong base address */        
    uint32_t txPingPongBaseAddr;    /* Tx ping/pong base address */
    uint16_t pingPongBufSz;         /* Tx/Rx ping/pong buffer size (bytes) */
    uint8_t i2sTxIntrPri;           /* I2S Tx interrupt priority */
    uint8_t i2sRxIntrPri;           /* I2S Rx interrupt priority */
    uint8_t i2sErrIntrPri;          /* I2S error interrupt priority */    

    /* Callback function for I2S Tx */
    PRUI2S_CallbackFxn i2sTxCallbackFxn;
    /* Callback function for I2S Rx */
    PRUI2S_CallbackFxn i2sRxCallbackFxn;
    /* Callback function for I2S errors */
    PRUI2S_CallbackFxn i2sErrCallbackFxn;    
} PRUI2S_Params;

/* IO Buffer for Write/Read */
typedef struct PRUI2S_IoBuf_s {
    void *ioBufAddr;                /* Interleaved buffer: Ch0 L, Ch1 L, ..., ChM L, Ch0 R, Ch1 R, ..., ChM R. */
} PRUI2S_IoBuf;

/* Conversion IO Buffer for Write/Read with conversion */
typedef struct PRUI2S_IoBufC_s {
    void *ioBufLAddr[PRUI2S_MAX_I2S];    /* Left channel buffers */
    void *ioBufRAddr[PRUI2S_MAX_I2S];    /* Right channel buffers */
} PRUI2S_IoBufC;

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
    uint8_t *pNumValidCfg
);

/* 
 *  De-initializes PRU I2S driver. 
 *
 *  Parameters: none
 *
 *  return: none
 */
void PRUI2S_deinit(void);

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

/**
 * \brief  This function configures INTC while preserving the existing configuration
 *
 * \param   handle              #PRUICSS_Handle returned from #PRUICSS_open()
 * \param   intcInitData        Pointer to structure of type
 *                              #PRUICSS_IntcInitData containing mapping
 *                              information
 *
 * \return  #SystemP_SUCCESS in case of success, #SystemP_FAILURE otherwise
 */
int32_t PRUICSS_intcCfg(
    PRUICSS_Handle handle,
    const PRUICSS_IntcInitData *intcInitData
);

/* Maps INTC system event to channel */
int32_t PRUICSS_intcSetSysEvtChMap(
    PRUICSS_Handle handle, 
    uint8_t sysevt,
    uint8_t channel
);
/**
 * @brief   This function sets System-Channel Map registers
 *
 * @param   sysevt         System event number
 * @param   channel        Host channel number
 * @param   polarity       Polarity of event
 * @param   type           Type of event
 * @param   baseaddr       Base address of PRUICSS
 *
 * @return   None
 */
static void PRUICSS_intcSetCmr(uint8_t sysevt,
                               uint8_t channel,
                               uint8_t polarity,
                               uint8_t type,
                               uintptr_t baseaddr);
/**
 * \brief    This function sets Channel-Host Map registers
 *
 * \param    channel         Channel number
 * \param    host            Host number
 * @param    baseaddr        Base address of PRUICSS
 *
 * \return   None
 */
static void PRUICSS_intcSetHmr(uint8_t channel,
                               uint8_t host,
                               uintptr_t baseaddr);

#endif /* _PRUICSS_DRV_AUX_H_ */
