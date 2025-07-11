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

#ifndef _PRUICSS_DRV_AUX_H_
#define _PRUICSS_DRV_AUX_H_

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
#include "pru_i2s_drv.h"
#include "pru_i2s_pruss_intc_mapping.h"  /* INTC configuration */
#include <pru_i2s/firmware/TDM4/icss_pru_i2s_fw.h>

#include <stdint.h>
#include <drivers/hw_include/tistdtypes.h>
#include <drivers/pruicss.h>

#define CONFIG_PRU_ICSS0 (0U)
/* Number of ICSS instances */
#define PRUI2S_NUM_ICSS_INST            ( 2 )
/* Number of PRU instances per ICSS */
#define PRUI2S_NUM_PRU_INST_PER_ICSS    ( 2 )

/* Maximum number of PRU I2S instances */
#define PRU_I2S_MAX_NUM_INST            ( PRUI2S_NUM_ICSS_INST * PRUI2S_NUM_PRU_INST_PER_ICSS )

/* Number of PRU images */
#define PRU_I2S_NUM_PRU_IMAGE           ( 2 )

/* Default interrupt priorities.
   VIM: 0(highest)..15(lowest). */
#define DEF_I2S_INTR_PRI                ( 0 )   /* I2S */
#define DEF_I2S_ERR_INTR_PRI            ( 0 )   /* I2S error */

/* Maximum VIM interrupt priority value */
#define MAX_VIM_INTR_PRI_VAL            ( 15 )

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

/* Support number of bits per I2S slot */
#define PRUI2S_BITS_PER_SLOT_16         ( 16 )  /* 16 bits per slot */
#define PRUI2S_BITS_PER_SLOT_32         ( 32 )  /* 32 bits per slot */

#define PRUI2S_MAX_PRU_PIN_NUM          ( 19 )  /* PRU maximum pin number */

/* Unlock and lock values for IOMUX -- copied from pinmux.c */
#define IOMUX_KICK_LOCK_VAL                     (0x00000000U)
#define IOMUX_KICK0_UNLOCK_VAL                  (0x83E70B13U)
#define IOMUX_KICK1_UNLOCK_VAL                  (0x95A4F1E0U)

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
/* PRUICSS Physical Instance IDs */
typedef enum PRUICSS_MaxInstances_s
{
   PRUICSS_INSTANCE_ONE=0,
   PRUICSS_INSTANCE_TWO=1,
   PRUICSS_INSTANCE_MAX=2
} PRUICSS_MaxInstances;
/* PRU I2S INTC configuration information */
typedef struct PRUI2S_IcssIntcCfgInfo_s {
    Bool init;  /* flag indicates whether INTC initialized for this PRU */

    /* Whether system event enabled */
    Bool sysevtEnabled[PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU];
    /* SYSEVT IDs - Range:0..159 */
    uint8_t sysevtId[PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU];
    /* SysEvt to Channel map. SYSEVTs - Range:0..159, Channels - Range:0..19 */
    PRUICSS_SysevtToChannelMap sysevtToChannelMap[PRUI2S_PRUICSS_INTC_NUM_SYSEVT_PER_PRU];
    /* Channel to Host map. Channels - Range:0..19, HOSTs - Range:0..19 */
    PRUICSS_ChannelToHostMap channelToHostMap[PRUI2S_PRUICSS_INTC_NUM_CHANNELS_PER_PRU];
    /*
     * { Host0..1   : PRU0/1,
     *   Host2..9   : PRUEVT_OUT0..7,
     *   Host10..11 : RTU_PRU0/1,
     *   Host12..19 : Task Managers }
     */
    uint32_t hostEnableBitmask;
} PRUI2S_IcssIntcCfgInfo;


/* PRU I2S pinmux attributes */
typedef struct PRUI2S_PruGpioPadAttrs_s {
    uint8_t pinNum;                 /* PRU Pin Number for FW */
    uint32_t padCfgRegAddr;         /* PAD configuration register */
    uint32_t padCfgRegVal;          /* PAD configuration register value */
} PRUI2S_PruGpioPadAttrs;

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

/* PRU I2S SW IP attributes */
static PRUI2S_SwipAttrs gPruI2sSwipAttrs[PRU_I2S_MAX_NUM_INST] =
{
#ifdef SOC_AM261X
{
    /* baseAddr */
    (uint32_t)CSL_ICSSM1_INTERNAL + (uint32_t)CSL_ICSS_M_DRAM0_SLV_RAM_REGS_BASE + (uint32_t)ICSS_PRUI2S_FW_REG_BASE,
    0,                                                                   /* icssInstId */
    PRUICSS_PRU0,                                                                       /* pruInstId */
    0,                                                                                  /* numTxI2s - 1 TX */
    0,                                                                                  /* numRxI2s - No RX */
    0,                                                                                  /* sampFreq */
    0,                                                                                  /* bitsPerSlot */
    CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM1_PR1_HOST_INTR_PEND_0,                             /* i2sTxHostIntNum */
    CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM1_PR1_HOST_INTR_PEND_1,                             /* i2sRxHostIntNum */
    CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM1_PR1_HOST_INTR_PEND_2,                             /* i2sErrHostIntNum */
    0,                                                                                  /* i2sTxIcssIntcSysEvt */
    0,                                                                                  /* i2sRxIcssIntcSysEvt */
    0,                                                                                  /* i2sErrIcssIntcSysEvt */
    /* bclkPin */
    {6, 0x531000AC, 0x00000500},  // GPIO43
    /* fsyncPin */
    {1, 0x531000B4, 0x00000500},  // GPIO45
    /* Tx pins */
    {
        {2, 0x53100068, 0x00000000},  // GPIO26 for TX
        {0, 0, 0},                     // Not used
        {0, 0, 0}                      // Not used
    },
    /* Rx pins - Not used for core 0 */
    {
        {0, 0, 0},
        {0, 0, 0}
    }
},
{
    /* baseAddr */
    (uint32_t)CSL_ICSSM1_INTERNAL + (uint32_t)CSL_ICSS_M_DRAM1_SLV_RAM_REGS_BASE + (uint32_t)ICSS_PRUI2S_FW_REG_BASE,
    0,                                                                   /* icssInstId */
    PRUICSS_PRU1,                                                                       /* pruInstId */
    0,                                                                                  /* numTxI2s - No TX */
    0,                                                                                  /* numRxI2s - 1 RX */
    0,                                                                                  /* sampFreq */
    0,                                                                                  /* bitsPerSlot */
    CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM1_PR1_HOST_INTR_PEND_3,                             /* i2sTxHostIntNum */
    CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM1_PR1_HOST_INTR_PEND_4,                             /* i2sRxHostIntNum */
    CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM1_PR1_HOST_INTR_PEND_5,                             /* i2sErrHostIntNum */
    0,                                                                                  /* i2sTxIcssIntcSysEvt */
    0,                                                                                  /* i2sRxIcssIntcSysEvt */
    0,                                                                                  /* i2sErrIcssIntcSysEvt */
    /* bclkPin */
    {0, 0x531000D8, 0x00000500},  // GPIO54
    /* fsyncPin */
    {1, 0x53100128, 0x00000500},  // GPIO74
    /* Tx pins - Not used for core 1 */
    {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    },
    /* Rx pins */
    {
        {4, 0x53100010, 0x00000500},  // GPIO4 for RX
        {0, 0, 0}                      // Not used
    }
}
#else
  {
        /* baseAddr */
        (uint32_t)CSL_ICSSM0_INTERNAL_U_BASE + (uint32_t)CSL_ICSS_M_DRAM0_SLV_RAM_REGS_BASE + (uint32_t)ICSS_PRUI2S_FW_REG_BASE,
        CONFIG_PRU_ICSS0,                                                                   /* icssInstId */
        PRUICSS_PRU0,                                                                       /* pruInstId */
        0,                                                                                  /* numTxI2s */
        0,                                                                                  /* numRxI2s */
        0,                                                                                  /* sampFreq */
        0,                                                                                  /* bitsPerSlot */
        CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_0,                             /* i2sTxHostIntNum */
        CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_1,                             /* i2sRxHostIntNum */
        CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_2,                             /* i2sErrHostIntNum */
        0,                                                                                  /* i2sTxIcssIntcSysEvt */
        0,                                                                                  /* i2sRxIcssIntcSysEvt */
        0,                                                                                  /* i2sErrIcssIntcSysEvt */
        /* bclkPin */
        {6, 0x5310016C, 0x00000500},
        /* fsyncPin */
        {1, 0x53100178, 0x00000500},
        /* Tx pins */
        {
            {2, 0x5310017C, 0x00000000}, 
            {3, 0x53100180, 0x00000000},
            {5, 0x5310015C, 0x00000000}
        },
        /* Rx pins */
        {
            {0, 0x53100174, 0x00000500}, 
            {4, 0x53100170, 0x00000500}
        }
    }, 
    {
        /* baseAddr */
        (uint32_t)CSL_ICSSM0_INTERNAL_U_BASE + (uint32_t)CSL_ICSS_M_DRAM1_SLV_RAM_REGS_BASE + (uint32_t)ICSS_PRUI2S_FW_REG_BASE,
        CONFIG_PRU_ICSS0,                                                                   /* icssInstId */
        PRUICSS_PRU1,                                                                       /* pruInstId */
        0,                                                                                  /* numTxI2s */
        0,                                                                                  /* numRxI2s */
        0,                                                                                  /* sampFreq */
        0,                                                                                  /* bitsPerSlot */
        CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_3,                             /* i2sTxHostIntNum */
        CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_4,                             /* i2sRxHostIntNum */
        CSLR_R5FSS0_CORE0_INTR_PRU_ICSSM0_PR1_HOST_INTR_PEND_5,                             /* i2sErrHostIntNum */
        0,                                                                                  /* i2sTxIcssIntcSysEvt */
        0,                                                                                  /* i2sRxIcssIntcSysEvt */
        0,                                                                                  /* i2sErrIcssIntcSysEvt */
        /* bclkPin */
        {0, 0x531001B4, 0x00000500},
        /* fsyncPin */
        {1, 0x531001B8, 0x00000500},
        /* Tx pins */
        {
            {4, 0x531001B0, 0x00000000}, 
            {5, 0x5310019C, 0x00000000}, 
            {6, 0x531001AC, 0x00000000}
        },
        /* Rx pins */
        {
            {2, 0x531001BC, 0x00000500}, 
            {3, 0x531001C0, 0x00000500}
        }
    }
#endif
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


/* Flag indicate whether ICSS INTC initialized */
static Bool gIcssIntcInitFlag[PRUI2S_NUM_ICSS_INST] = {FALSE, FALSE};
/* PRU I2S ICSSG configuration info */
static PRUI2S_IcssIntcCfgInfo gPruIcssIntcCfgInfo[PRUI2S_NUM_ICSS_INST][PRUI2S_NUM_PRU_INST_PER_ICSS] =
{
    {
        /* ICSS0 */
        {
            /* PRU0 */
            FALSE, 
            {FALSE, FALSE, FALSE}, 
            {PRU_ARM_EVENT02, PRU_ARM_EVENT03, PRU_ARM_EVENT04}, 
            {{PRU_ARM_EVENT02, CHANNEL2, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT03, CHANNEL3, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT04, CHANNEL4, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE}},
            {{CHANNEL2, PRU_EVTOUT0}, {CHANNEL3, PRU_EVTOUT1}, {CHANNEL4, PRU_EVTOUT2}},
            PRU_EVTOUT0_HOSTEN_MASK | PRU_EVTOUT1_HOSTEN_MASK | PRU_EVTOUT2_HOSTEN_MASK
        },
        {
            /* PRU1 */
            FALSE, 
            {FALSE, FALSE, FALSE}, 
            {PRU_ARM_EVENT05, PRU_ARM_EVENT06, PRU_ARM_EVENT07}, 
            {{PRU_ARM_EVENT05, CHANNEL5, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT06, CHANNEL6, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT07, CHANNEL7, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE}},
            {{CHANNEL5, PRU_EVTOUT3}, {CHANNEL6, PRU_EVTOUT4}, {CHANNEL7, PRU_EVTOUT5}},
            PRU_EVTOUT3_HOSTEN_MASK | PRU_EVTOUT4_HOSTEN_MASK | PRU_EVTOUT5_HOSTEN_MASK
        }
    },
    {
        /* ICSS1 */
        {
            /* PRU0 */
            FALSE, 
            {FALSE, FALSE, FALSE}, 
            {PRU_ARM_EVENT02, PRU_ARM_EVENT03, PRU_ARM_EVENT04}, 
            {{PRU_ARM_EVENT02, CHANNEL2, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT03, CHANNEL3, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT04, CHANNEL4, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE}},
            {{CHANNEL2, PRU_EVTOUT0}, {CHANNEL3, PRU_EVTOUT1}, {CHANNEL4, PRU_EVTOUT2}},
            PRU_EVTOUT0_HOSTEN_MASK | PRU_EVTOUT1_HOSTEN_MASK | PRU_EVTOUT2_HOSTEN_MASK
        },
        {
            /* PRU1 */
            FALSE, 
            {FALSE, FALSE, FALSE}, 
            {PRU_ARM_EVENT05, PRU_ARM_EVENT06, PRU_ARM_EVENT07}, 
            {{PRU_ARM_EVENT05, CHANNEL5, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT06, CHANNEL6, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE},
             {PRU_ARM_EVENT07, CHANNEL7, SYS_EVT_POLARITY_HIGH, SYS_EVT_TYPE_PULSE}},
            {{CHANNEL5, PRU_EVTOUT3}, {CHANNEL5, PRU_EVTOUT4}, {CHANNEL6, PRU_EVTOUT5}},
            PRU_EVTOUT3_HOSTEN_MASK | PRU_EVTOUT4_HOSTEN_MASK | PRU_EVTOUT5_HOSTEN_MASK
        }
    }    
};

//#define _DBG_TX_PP_CAP
#ifdef _DBG_TX_PP_CAP
/* debug */
#define TX_PP_CAP_BUF_SZ ( 360 )
__attribute__((section(".TxPpBufCap"))) uint16_t gTxPpCapBufIdx=0;
__attribute__((section(".TxPpBufCap"))) uint8_t gTxPpSelCapBuf[TX_PP_CAP_BUF_SZ];
__attribute__((section(".TxPpBufCap"))) uint32_t gTxPpSrcAddrCapBuf[TX_PP_CAP_BUF_SZ];
__attribute__((section(".TxPpBufCap"))) uint32_t gTxPpDstAddrCapBuf[TX_PP_CAP_BUF_SZ];
#endif

//#define _DBG_RX_PP_CAP
#ifdef _DBG_RX_PP_CAP
/* debug */
#define RX_PP_CAP_BUF_SZ ( 360 )
__attribute__((section(".RxPpBufCap"))) uint16_t gRxPpCapBufIdx=0;
__attribute__((section(".RxPpBufCap"))) uint8_t gRxPpSelCapBuf[RX_PP_CAP_BUF_SZ];
__attribute__((section(".RxPpBufCap"))) uint32_t gRxPpSrcAddrCapBuf[RX_PP_CAP_BUF_SZ];
__attribute__((section(".RxPpBufCap"))) uint32_t gRxPpDstAddrCapBuf[RX_PP_CAP_BUF_SZ];
#endif

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
 *      gPruFwImageInfo: Pointer to PRU FW image information
 *
 *  return: status code 
 */
int32_t PRUI2S_init(
    uint8_t *pNumValidCfg, PRUI2S_PruFwImageInfo (*gPruFwImageInfo)[2]
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

/**
 * \brief    Initializes ICSS INTC for PRU I2S
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_initIcssIntc(
    PRUI2S_Config *pCfg
);

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
 * \brief    Initializes SoC PADs for PRU I2S FW
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   None
 */
static void PRUI2S_initFwPadRegs(
    PRUI2S_Config *pCfg
);

/**
 * \brief    Configures PAD register
 *
 * \param    pPruGpioPadAttrs   Pointer to PRU GPIO pad attributes structure
 *
 * \return   None
 */
static void PRUI2S_pruGpioPadConfig(
    PRUI2S_PruGpioPadAttrs *pPruGpioPadAttrs
);

/**
 * \brief    Configures PRU GPIO Output Control register for PRU I2S FW
 *
 * \param    pCfg               Pointer to PRU I2S configuration structure
 *
 * \return   int32_t            Status of the operation
 */
static int32_t PRUI2S_initPruGpioOutCtrlReg(
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

/**
 * \brief    Unlock pinmux
 *
 * \param    domainId           Domain ID
 *
 * \return   None
 */
static void Pinmux_unlockMMR(uint32_t domainId);

/**
 * \brief    Lock pinmux
 *
 * \param    domainId           Domain ID
 *
 * \return   None
 */
static void Pinmux_lockMMR(uint32_t domainId);
#endif /* _PRUICSS_DRV_AUX_H_ */
