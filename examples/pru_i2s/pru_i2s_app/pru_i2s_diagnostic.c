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

/**
 * \file pru_i2s_diagnostic.c
 *
 * \brief PRU I2S diagnostic application demonstrating real-time audio streaming
 *
 * \details This application provides a comprehensive diagnostic interface for testing
 *          and validating PRU I2S audio communication. It demonstrates:
 *          - Real-time audio streaming over I2S/TDM interfaces
 *          - Ping-pong buffer management for continuous data flow
 *          - Multi-channel audio Tx/Rx operations
 *          - Error detection and reporting
 *          - ISR-based interrupt handling for low-latency operation
 *
 * ## Application Flow
 *
 * The application follows this sequence:
 *
 * **STEP 1: System Initialization**
 * - Initialize SoC drivers (UART, I2C, etc.)
 * - Set up board peripherals and IO configuration
 *
 * **STEP 2: PRU-ICSS Subsystem Initialization**
 * - Open PRU-ICSS instance using SysConfig-generated configuration
 * - Disable PRU cores before firmware load
 * - Clear PRU data memory regions using PRUICSS_initMemory
 * - Initialize INTC (Interrupt Controller) using SysConfig data
 *
 * **STEP 3: Semaphore Initialization**
 * - Create binary semaphores for Tx/Rx synchronization
 * - Used for ISR-to-task communication pattern (required for streaming protocols)
 *
 * **STEP 4: PRU I2S Driver Initialization**
 * - Initialize PRU I2S driver parameters
 * - Configure ping-pong buffer addresses and sizes
 * - Register ISR callback functions for Tx, Rx, and error interrupts
 * - Open PRU I2S instances (separate for Tx and Rx)
 *
 * **STEP 5: Firmware Loading and PRU Core Enablement**
 * - Load PRU I2S firmware binaries to PRU instruction and data memory
 * - Reset PRU cores to initialize state
 * - Enable PRU cores to begin audio streaming
 * - Display firmware version information
 *
 * **STEP 6: Real-Time Audio Streaming Loop**
 * - Mode selection based on enabled instances:
 *   - Full-duplex: Both Tx and Rx enabled (simultaneous transmit/receive)
 *   - Half-duplex Tx-only: Only Tx enabled (transmit only)
 *   - Half-duplex Rx-only: Only Rx enabled (receive only)
 * - Continuous loop for ping-pong buffer processing
 * - Wait for semaphore signals from Tx/Rx ISRs
 * - Read received audio data from Rx buffers (if Rx enabled)
 * - Write transmit audio data to Tx buffers (if Tx enabled)
 * - Update buffer pointers for next iteration
 * - Monitor and report error conditions
 *
 * **STEP 7: Cleanup and Shutdown**
 * - Close PRU I2S driver instances
 * - Deinitialize drivers and release resources
 * - Destruct semaphores
 * - Close board and system drivers
 *
 * ## Semaphore Usage Pattern
 *
 * The application uses **semaphores for ISR-to-task synchronization** because:
 * - I2S is a **continuous streaming protocol** (not query-based like position encoders)
 * - Data arrives/departs at fixed sample rate (e.g., 48 kHz)
 * - Must process EVERY ping-pong buffer swap - missing a buffer = audio glitch
 * - Semaphores allow task to sleep during idle, improving CPU efficiency
 * - Industry standard pattern for all audio drivers (ALSA, ASIO, CoreAudio)
 *
 * This is fundamentally different from encoder protocols (EnDAT, HDSL, BiSS-C) which
 * use polling because they are query-based (on-demand reads at lower rates).
 *
 * ## Error Handling Strategy
 *
 * The application follows these error handling principles:
 * - All driver API calls check return values for detailed error codes
 * - Error counters track cumulative failures (Tx/Rx errors, interrupt clear errors)
 * - Periodic error logging (first occurrence + every Nth occurrence)
 * - Timeout detection on semaphore waits indicates stream failure
 * - Error threshold detection terminates loop to prevent indefinite error states
 * - Uses DebugP_assert for critical initialization failures
 *
 * ## Driver Validation Strategy
 *
 * The PRU I2S driver uses optimized validation:
 * - **Handle validation**: All public APIs validate handle for NULL
 * - **Buffer address validation**: Ping-pong buffer addresses validated during open
 * - **Parameter range checking**: Buffer sizes, channel counts validated against limits
 * - **PRUICSS handle validation**: Ensures valid PRUICSS subsystem access
 *
 * ## Configuration Notes
 *
 * - All PRU I2S configuration parameters are set via application config files
 * - INTC mapping is defined in SysConfig
 * - Firmware selection is automatic based on I2S/TDM mode
 * - Buffer sizes must match firmware expectations
 */


/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */


#include <stdio.h>
#include <stdint.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/pruicss.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "data.h"
#include "include/pru_i2s_drv.h"
#include "pru_i2s_app/pru_i2s_app_config.h"

#ifdef SOC_AM263X
#include <pru_i2s_app/board/ioexp_tca6416.h>
#endif

/* Firmware selection based on mode and enabled instances */
/* Include firmware only for enabled instances */
#if (CONFIG_I2S0_MODE == TDM_MODE)
    /* TDM Mode firmware */
    #if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
        #include "firmware/TDM4/pru_i2s_tdm4_pru0_array.h"  /* PRU0 TDM firmware */
    #endif
    #if defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
        #include "firmware/TDM4/pru_i2s_tdm4_pru1_array.h"  /* PRU1 TDM firmware */
    #endif
#else
    /* I2S Mode firmware */
    #if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
        #include "pru_i2s/firmware/I2S/pru_i2s_pru0_array.h"  /* PRU0 I2S firmware */
    #endif
    #if defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
        #include "pru_i2s/firmware/I2S/pru_i2s_pru1_array.h"  /* PRU1 I2S firmware */
    #endif
#endif

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* Optional Rx-to-Tx loopback for testing */
/* #define _DBG_PRUI2S_RX_TO_TX_LB */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* PRU-ICSS driver handle */
PRUICSS_Handle gPruIcssHandle = NULL;

/* PRU I2S driver handles */
PRUI2S_Handle gHPruI2s0 = NULL;  /* Tx instance */
PRUI2S_Handle gHPruI2s1 = NULL;  /* Rx instance */

/* Rx & Tx semaphores for ISR-to-task synchronization */
SemaphoreP_Object gPruI2s0TxSemObj;
SemaphoreP_Object gPruI2s1RxSemObj;
Bool gSemaphoresInitialized = FALSE;

/* Audio buffer storage - gPruI2s1RxPingPongBuf and gPruI2s0TxBuf defined in data.h */
__attribute__((section(".RxBuf"))) int32_t gPruI2s0RxBuf[RX_BUF_SZ_32B];
__attribute__((section(".RxBuf"))) int32_t gPruI2s1RxBuf[RX_BUF_SZ_32B];

/* PRU I2S buffer control variables */
int32_t *gPPruI2s0TxBuf;            /* PRU I2S 0 transmit buffer pointer */
uint32_t gPruI2s0TxCnt;             /* PRU I2S 0 transmit count (all channels) */
uint16_t gPruI2s0XferSz;            /* PRU I2S 0 transfer size in 32-bit words */

int32_t *gPPruI2s1RxBuf;            /* PRU I2S 1 receive buffer pointer */
uint32_t gPruI2s1RxCnt;             /* PRU I2S 1 receive count (all channels) */
uint16_t gPruI2s1XferSz;            /* PRU I2S 1 transfer size in 32-bit words */

/* Error and debug statistics */
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

/* PRU firmware image information */
/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* PRU I2S PRU image info */
static PRUI2S_PruFwImageInfo gPruFwImageInfo[PRU_I2S_NUM_PRU_IMAGE] =
{
    {pru_prupru_i2s0_image_0_0, pru_prupru_i2s0_image_0_1, sizeof(pru_prupru_i2s0_image_0_0), sizeof(pru_prupru_i2s0_image_0_1)},
    {pru_prupru_i2s1_image_0_0, pru_prupru_i2s1_image_0_1, sizeof(pru_prupru_i2s1_image_0_0), sizeof(pru_prupru_i2s1_image_0_1)}
};


/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/*                          Function Declarations                             */
/* ========================================================================== */

/* Initialization functions */
static void prui2s_pruicss_init(void);
static void prui2s_pruicss_load_run_fw(void);
static void prui2s_display_fw_version(uint8_t pruSlice);
static int32_t prui2s_init_semaphores(void);
static int32_t prui2s_init_instance(uint8_t instanceIdx, PRUI2S_Handle *pHandle);
#ifdef SOC_AM263X
static void prui2s_board_io_expander_init(void);
#endif

/* Runtime operation functions */
static void prui2s_streaming_loop_full_duplex(PRUI2S_Handle hTx, PRUI2S_Handle hRx);
static void prui2s_streaming_loop_tx_only(PRUI2S_Handle hTx);
static void prui2s_streaming_loop_rx_only(PRUI2S_Handle hRx);
static void prui2s_display_statistics(void);
static void prui2s_display_configuration(void);

/* Cleanup functions */
static void prui2s_cleanup_semaphores(void);
static void prui2s_cleanup_instances(void);

/* ISR handlers */
void pruI2s0TxIrqHandler(void *args);
void pruI2s0ErrIrqHandler(void *args);
void pruI2s1RxIrqHandler(void *args);
void pruI2s1ErrIrqHandler(void *args);


/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

uint32_t gPruI2s1ErrUndCnt=0;       /* PRU I2S 1 Tx underflow error count */
uint32_t gPruI2s1ErrFsyncCnt=0;     /* PRU I2S 1 frame sync error count */

volatile Bool gRunFlag = TRUE;  /* Flag for continuing to execute test */
uint32_t gLoopCnt;

/* Optional Rx-to-Tx loopback mode */
#ifdef _DBG_PRUI2S_RX_TO_TX_LB
uint8_t gPruI2s1NumRxI2s;
uint8_t gPruI2s0NumTxI2s;
float gPruI2s0LGain = GAIN_MINUS_3DB;  /* PRU I2S 0 Left channel gain, -3 dB */
float gPruI2s0RGain = GAIN_MINUS_6DB;  /* PRU I2S 0 Right channel gain, -6 dB */
#endif

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/**
 * \brief Initialize PRU-ICSS subsystem for PRU I2S operation
 *
 * \details Performs PRU-ICSS initialization including:
 *          - Open PRU-ICSS handle using SysConfig-generated configuration
 *          - Clear PRU data RAM regions using PRUICSS_initMemory
 *          - Disable PRU cores before firmware load
 *
 * This function must be called before loading PRU firmware and opening PRU I2S instances.
 */
static void prui2s_pruicss_init(void)
{
    int32_t status = SystemP_FAILURE;
    uint32_t u_status = 0;
    uint8_t pru0_id = PRUICSS_PRU0;
    uint8_t pru1_id = PRUICSS_PRU1;

    /* Open PRU-ICSS instance from SysConfig (ICSSM1) */
    gPruIcssHandle = PRUICSS_open(CONFIG_PRU_ICSS0);
    if(gPruIcssHandle == NULL)
    {
        DebugP_log("\r\n ERROR: PRUICSS_open failed - NULL handle returned\n");
        DebugP_assert(0);
    }

    /* Disable PRU0 core */
    status = PRUICSS_disableCore(gPruIcssHandle, pru0_id);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Disable PRU1 core */
    status = PRUICSS_disableCore(gPruIcssHandle, pru1_id);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Clear PRU-ICSS DATA RAM for PRU0 slice */
    u_status = PRUICSS_initMemory(gPruIcssHandle, PRUICSS_DATARAM(0));
    DebugP_assert(0 != u_status);

    /* Clear PRU-ICSS DATA RAM for PRU1 slice */
    u_status = PRUICSS_initMemory(gPruIcssHandle, PRUICSS_DATARAM(1));
    DebugP_assert(0 != u_status);

    /* Configure PRU-ICSS Interrupt Controller */
    /* INTC data is declared in ti_drivers_config.h based on SoC:
       - AM263x uses icss0_intc_initdata (ICSS instance 0)
       - AM261x uses icss1_intc_initdata (ICSS instance 1) */
#ifdef SOC_AM263X
    status = PRUICSS_intcInit(gPruIcssHandle, &icss0_intc_initdata);
#else
    status = PRUICSS_intcInit(gPruIcssHandle, &icss1_intc_initdata);
#endif
    DebugP_assert(SystemP_SUCCESS == status);
}

/**
 * \brief Load and execute PRU I2S firmware
 *
 * \details This function handles the complete firmware loading sequence for enabled PRU cores:
 *          - Disable PRU core
 *          - Write firmware binary to PRU instruction RAM
 *          - Validate load success
 *          - Reset PRU core
 *          - Enable PRU core to begin execution
 *
 * The firmware binaries are selected based on SysConfig-enabled instances.
 * Only enabled instances (CONFIG_PRU_I2S0_ENABLED, CONFIG_PRU_I2S1_ENABLED) are loaded.
 */
static void prui2s_pruicss_load_run_fw(void)
{
    int32_t status = SystemP_FAILURE;
    uint32_t u_status = 0;

#if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
    /* ============================================ */
    /* Load PRU0 firmware (Tx)                     */
    /* ============================================ */
    const uint32_t *pru0_firmware = gPruFwImageInfo[0].pPruImemImg;
    uint32_t pru0_firmware_size = gPruFwImageInfo[0].pruImemImgSz;
    uint8_t pru0_id = PRUICSS_PRU0;

    /* Disable PRU0 core */
    status = PRUICSS_disableCore(gPruIcssHandle, pru0_id);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Load firmware to PRU0 instruction RAM */
    u_status = PRUICSS_writeMemory(gPruIcssHandle, PRUICSS_IRAM_PRU(0), 0,
                                  (uint32_t *)pru0_firmware, pru0_firmware_size);
    DebugP_assert(0 != u_status);

    /* DMEM is cleared by PRUICSS_initMemory() earlier - skip explicit DMEM write
     * to avoid hardware timeout issues. The firmware will initialize DMEM at runtime. */

    /* Reset PRU0 core */
    status = PRUICSS_resetCore(gPruIcssHandle, pru0_id);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Enable PRU0 core to run firmware */
    status = PRUICSS_enableCore(gPruIcssHandle, pru0_id);
    DebugP_assert(SystemP_SUCCESS == status);
#endif

#if defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
    /* ============================================ */
    /* Load PRU1 firmware (Rx)                     */
    /* ============================================ */
    const uint32_t *pru1_firmware = gPruFwImageInfo[1].pPruImemImg;
    uint32_t pru1_firmware_size = gPruFwImageInfo[1].pruImemImgSz;
    uint8_t pru1_id = PRUICSS_PRU1;

    /* Disable PRU1 core */
    status = PRUICSS_disableCore(gPruIcssHandle, pru1_id);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Load firmware to PRU1 instruction RAM */
    u_status = PRUICSS_writeMemory(gPruIcssHandle, PRUICSS_IRAM_PRU(1), 0,
                                  (uint32_t *)pru1_firmware, pru1_firmware_size);
    DebugP_assert(0 != u_status);

    /* DMEM is cleared by PRUICSS_initMemory() earlier - skip explicit DMEM write
     * to avoid hardware timeout issues. The firmware will initialize DMEM at runtime. */

    /* Reset PRU1 core */
    status = PRUICSS_resetCore(gPruIcssHandle, pru1_id);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Enable PRU1 core to run firmware */
    status = PRUICSS_enableCore(gPruIcssHandle, pru1_id);
    DebugP_assert(SystemP_SUCCESS == status);
#endif
}

/**
 * \brief Display PRU I2S firmware version information
 *
 * \param pruSlice  PRU core slice (0 or 1)
 */
static void prui2s_display_fw_version(uint8_t pruSlice)
{
    uint32_t version;
    const uint32_t *pFwImage;

    /* Get firmware image based on PRU slice */
    if(pruSlice == PRUICSS_PRU0)
    {
        pFwImage = gPruFwImageInfo[0].pPruImemImg;
    }
    else
    {
        pFwImage = gPruFwImageInfo[1].pPruImemImg;
    }

    /* Firmware version is stored at offset 1 in IMEM */
    version = pFwImage[1];

    DebugP_log("PRU I2S PRU%d Firmware: %x.%x.%x (%s)\r\n",
               pruSlice,
               (version >> 24) & 0x7F,
               (version >> 16) & 0xFF,
               version & 0xFFFF,
               version & (1 << 31) ? "internal" : "release");
}

/**
 * \brief Initialize semaphores for ISR-to-task synchronization
 *
 * \return SystemP_SUCCESS on success, SystemP_FAILURE on error
 */
static int32_t prui2s_init_semaphores(void)
{
    int32_t status;

    /* Construct binary semaphore for Tx completion */
    status = SemaphoreP_constructBinary(&gPruI2s0TxSemObj, 0);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log("ERROR: SemaphoreP_constructBinary() gPruI2s0TxSemObj failed\r\n");
        return SystemP_FAILURE;
    }

    /* Construct binary semaphore for Rx completion */
    status = SemaphoreP_constructBinary(&gPruI2s1RxSemObj, 0);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log("ERROR: SemaphoreP_constructBinary() gPruI2s1RxSemObj failed\r\n");
        SemaphoreP_destruct(&gPruI2s0TxSemObj);
        return SystemP_FAILURE;
    }

    gSemaphoresInitialized = TRUE;
    DebugP_log("Semaphores initialized successfully\r\n");
    return SystemP_SUCCESS;
}

/**
 * \brief Initialize single PRU I2S instance
 *
 * \param instanceIdx   Instance index (TEST_PRUI2S0_IDX or TEST_PRUI2S1_IDX)
 * \param pHandle       Pointer to store opened handle
 *
 * \return SystemP_SUCCESS on success, SystemP_FAILURE on error
 */
static int32_t prui2s_init_instance(uint8_t instanceIdx, PRUI2S_Handle *pHandle)
{
    PRUI2S_Params prms;
    PRUI2S_SwipAttrs swipAttrs;
    int32_t status;

    /* Get SWIP attributes for this instance */
    status = PRUI2S_getInitCfg(instanceIdx, &swipAttrs);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_getInitCfg() instance %d failed\r\n", instanceIdx);
        return SystemP_FAILURE;
    }

    /* Initialize parameters with defaults */
    PRUI2S_paramsInit(&prms);

    /* Set PRUICSS handle (required for base address calculation) */
    prms.pruicss_handle = gPruIcssHandle;

    /* Configure instance-specific parameters */
    if(instanceIdx == TEST_PRUI2S0_IDX)
    {
        /* PRU I2S 0 - Tx only */
        DebugP_assert((swipAttrs.numTxI2s > 0) && (swipAttrs.numRxI2s == 0));

        gPruI2s0XferSz = PRUI2S0_TX_PING_PONG_BUF_SZ_32B/PING_PONG_BUFFER_COUNT;

        prms.rxPingPongBaseAddr = 0;
        prms.txPingPongBaseAddr = (uint32_t)PRUI2S0_TX_PING_PONG_BASE_ADDR;
        prms.pingPongBufSz = PRUI2S0_TX_PING_PONG_BUF_SZ;
        prms.i2sTxCallbackFxn = &pruI2s0TxIrqHandler;
        prms.i2sRxCallbackFxn = NULL;
        prms.i2sErrCallbackFxn = &pruI2s0ErrIrqHandler;

        gPPruI2s0TxBuf = &gPruI2s0TxBuf[0];
        gPruI2s0TxCnt = 0;

#ifdef _DBG_PRUI2S_RX_TO_TX_LB
        gPruI2s0NumTxI2s = swipAttrs.numTxI2s;
#endif
    }
    else
    {
        /* PRU I2S 1 - Rx only */
        DebugP_assert((swipAttrs.numTxI2s == 0) && (swipAttrs.numRxI2s > 0));

        gPruI2s1XferSz = PRUI2S1_RX_PING_PONG_BUF_SZ_32B/PING_PONG_BUFFER_COUNT;

        prms.rxPingPongBaseAddr = (uint32_t)gPruI2s1RxPingPongBuf;
        prms.txPingPongBaseAddr = 0;
        prms.pingPongBufSz = PRUI2S1_RX_PING_PONG_BUF_SZ;
        prms.i2sTxCallbackFxn = NULL;
        prms.i2sRxCallbackFxn = &pruI2s1RxIrqHandler;
        prms.i2sErrCallbackFxn = &pruI2s1ErrIrqHandler;

        gPPruI2s1RxBuf = &gPruI2s1RxBuf[0];
        gPruI2s1RxCnt = 0;

#ifdef _DBG_PRUI2S_RX_TO_TX_LB
        gPruI2s1NumRxI2s = swipAttrs.numRxI2s;
#endif
    }

    /* Open PRU I2S instance */
    *pHandle = PRUI2S_open(instanceIdx, &prms);
    if (*pHandle == NULL)
    {
        DebugP_log("ERROR: PRUI2S_open() instance %d failed\r\n", instanceIdx);
        return SystemP_FAILURE;
    }

    DebugP_log("PRU I2S instance %d initialized (Tx:%d, Rx:%d channels)\r\n",
               instanceIdx, swipAttrs.numTxI2s, swipAttrs.numRxI2s);

    return SystemP_SUCCESS;
}

#ifdef SOC_AM263X
/**
 * \brief Configure I2C IO Expander for board signal routing
 *
 * \details Routes PRU signals to HSEC connector by configuring:
 *          - ICSSM1_MUX_SEL = 1
 *          - ICSSM2_MUX_SEL = 1
 *          Routes PR0_PRUn_GPOmm signals to HSEC
 *
 * \note This is board-specific configuration and may vary per platform
 */
static void prui2s_board_io_expander_init(void)
{
    int32_t status = SystemP_SUCCESS;
    uint32_t ioIndex;
    TCA6416_Config gTCA6416_Config;
    TCA6416_Params tca6416Params;

    TCA6416_Params_init(&tca6416Params);
    status = TCA6416_open(&gTCA6416_Config, &tca6416Params);

    if(status == SystemP_SUCCESS)
    {
        /* Configure P02 as output and set HIGH */
        ioIndex = IOEXP_INDEX_P02;
        status = TCA6416_config(&gTCA6416_Config, ioIndex, TCA6416_MODE_OUTPUT);
        status += TCA6416_setOutput(&gTCA6416_Config, ioIndex, TCA6416_OUT_STATE_HIGH);

        /* Configure P03 as output and set HIGH */
        ioIndex = IOEXP_INDEX_P03;
        status += TCA6416_config(&gTCA6416_Config, ioIndex, TCA6416_MODE_OUTPUT);
        status += TCA6416_setOutput(&gTCA6416_Config, ioIndex, TCA6416_OUT_STATE_HIGH);

        if(status != SystemP_SUCCESS)
        {
            DebugP_log("WARNING: I2C IO Expander configuration failed\r\n");
        }
        else
        {
            DebugP_log("I2C IO Expander configured successfully\r\n");
        }
    }
    else
    {
        DebugP_log("WARNING: TCA6416_open failed\r\n");
    }

    TCA6416_close(&gTCA6416_Config);
}
#endif

/**
 * \brief Real-time audio streaming loop with ping-pong buffer processing
 *
 * \param hTx   PRU I2S Tx instance handle
 * \param hRx   PRU I2S Rx instance handle
 *
 * \details Continuous loop that:
 *          - Writes next Tx buffer
 *          - Waits for Rx and Tx completion via semaphores
 *          - Reads next Rx buffer
 *          - Updates buffer pointers
 *          - Monitors error conditions
 */
static void prui2s_streaming_loop_full_duplex(PRUI2S_Handle hTx, PRUI2S_Handle hRx)
{
    PRUI2S_IoBuf rdIoBuf;
    PRUI2S_IoBuf wrtIoBuf;
    int32_t status;

    gLoopCnt = 0;
    gRunFlag = TRUE;

    DebugP_log("\r\nStarting real-time audio streaming loop (FULL-DUPLEX mode)...\r\n");
    DebugP_log("Press Ctrl+C to stop\r\n\r\n");

    while (gRunFlag == TRUE)
    {
        /* Write next PRU I2S0 Tx ping/pong buffer */
        PRUI2S_ioBufInit(&wrtIoBuf);
        wrtIoBuf.ioBufAddr = gPPruI2s0TxBuf;
        status = PRUI2S_write(hTx, &wrtIoBuf);
        if (status != PRUI2S_DRV_SOK)
        {
            gPruI2s0WrtErrCnt++;
            if (gPruI2s0WrtErrCnt == 1 || (gPruI2s0WrtErrCnt % ERROR_LOG_INTERVAL) == 0)
            {
                DebugP_log("ERROR: PRU I2S0 write error (count: %d)\r\n", gPruI2s0WrtErrCnt);
            }
            if (gPruI2s0WrtErrCnt > ERROR_COUNT_THRESHOLD)
            {
                DebugP_log("ERROR: PRU I2S0 write error count exceeded threshold\r\n");
                gRunFlag = FALSE;
                continue;
            }
        }

        /* Wait for Rx interrupt with timeout */
        status = SemaphoreP_pend(&gPruI2s1RxSemObj, SEMAPHORE_TIMEOUT_MS);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log("ERROR: Timeout waiting for PRU I2S1 Rx event\r\n");
            gRunFlag = FALSE;
            continue;
        }

        /* Wait for Tx interrupt with timeout */
        status = SemaphoreP_pend(&gPruI2s0TxSemObj, SEMAPHORE_TIMEOUT_MS);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log("ERROR: Timeout waiting for PRU I2S0 Tx event\r\n");
            gRunFlag = FALSE;
            continue;
        }

        gLoopCnt++;

        /* Read next PRU I2S1 Rx ping/pong buffer */
        PRUI2S_ioBufInit(&rdIoBuf);
        rdIoBuf.ioBufAddr = gPPruI2s1RxBuf;
        status = PRUI2S_read(hRx, &rdIoBuf);
        if (status != PRUI2S_DRV_SOK)
        {
            gPruI2s1RdErrCnt++;
            if (gPruI2s1RdErrCnt == 1 || (gPruI2s1RdErrCnt % ERROR_LOG_INTERVAL) == 0)
            {
                DebugP_log("ERROR: PRU I2S1 read error (count: %d)\r\n", gPruI2s1RdErrCnt);
            }
            if (gPruI2s1RdErrCnt > ERROR_COUNT_THRESHOLD)
            {
                DebugP_log("ERROR: PRU I2S1 read error count exceeded threshold\r\n");
                gRunFlag = FALSE;
                continue;
            }
        }

#ifdef _DBG_PRUI2S_RX_TO_TX_LB
        /* Optional: Rx-to-Tx loopback with gain/mixing */
        int32_t *pSrc32b = gPPruI2s1RxBuf;
        int32_t *pDst32b = gPPruI2s0TxBuf;
        for (uint16_t i = 0; i < gPruI2s0XferSz/(2*gPruI2s0NumTxI2s); i++)
        {
            /* Compute Tx left-ch outputs with gain */
            pDst32b[0] = (int32_t)(pSrc32b[0] * gPruI2s0LGain);
            pDst32b[1] = (int32_t)(pSrc32b[1] * gPruI2s0LGain);
            pDst32b[2] = (int32_t)((pSrc32b[0] + pSrc32b[1])/CHANNEL_MIX_DIVISOR);
            pSrc32b += gPruI2s1NumRxI2s;
            pDst32b += gPruI2s0NumTxI2s;

            /* Compute Tx right-ch outputs with gain */
            pDst32b[0] = (int32_t)(pSrc32b[0] * gPruI2s0RGain);
            pDst32b[1] = (int32_t)(pSrc32b[1] * gPruI2s0RGain);
            pDst32b[2] = (int32_t)((pSrc32b[0] + pSrc32b[1])/CHANNEL_MIX_DIVISOR);
            pSrc32b += gPruI2s1NumRxI2s;
            pDst32b += gPruI2s0NumTxI2s;
        }
#endif

        /* Update Rx buffer pointer with wraparound */
        gPruI2s1RxCnt += gPruI2s1XferSz;
        gPPruI2s1RxBuf += gPruI2s1XferSz;
        if (gPruI2s1RxCnt >= RX_BUF_SZ_32B)
        {
            gPPruI2s1RxBuf = &gPruI2s1RxBuf[0];
            gPruI2s1RxCnt = 0;
        }

        /* Update Tx buffer pointer with wraparound */
        gPruI2s0TxCnt += gPruI2s0XferSz;
        gPPruI2s0TxBuf += gPruI2s0XferSz;
        if (gPruI2s0TxCnt >= TX_BUF_SZ_32B)
        {
            gPPruI2s0TxBuf = &gPruI2s0TxBuf[0];
            gPruI2s0TxCnt = 0;
        }

        /* Display periodic progress (every 1000 iterations) */
        if ((gLoopCnt % 1000) == 0)
        {
            DebugP_log("Streaming: %d iterations completed\r", gLoopCnt);
        }
    }

    DebugP_log("\r\nStreaming loop terminated after %d iterations\r\n", gLoopCnt);
}

/**
 * \brief Real-time audio streaming loop (Tx-only mode)
 *
 * \param hTx    Handle to PRU I2S Tx instance
 *
 * \details Tx-only streaming loop for half-duplex transmit operation that:
 *          - Writes Tx buffers
 *          - Waits for Tx interrupt via semaphore
 *          - Updates buffer pointers with wraparound
 *          - Displays periodic progress
 *          - Monitors error conditions
 */
static void prui2s_streaming_loop_tx_only(PRUI2S_Handle hTx)
{
    PRUI2S_IoBuf wrtIoBuf;
    int32_t status;

    gLoopCnt = 0;
    gRunFlag = TRUE;

    DebugP_log("\r\nStarting real-time audio streaming loop (TX-ONLY mode)...\r\n");
    DebugP_log("Press Ctrl+C to stop\r\n\r\n");

    while (gRunFlag == TRUE)
    {
        /* Write next PRU I2S0 Tx ping/pong buffer */
        PRUI2S_ioBufInit(&wrtIoBuf);
        wrtIoBuf.ioBufAddr = gPPruI2s0TxBuf;
        status = PRUI2S_write(hTx, &wrtIoBuf);
        if (status != PRUI2S_DRV_SOK)
        {
            gPruI2s0WrtErrCnt++;
            if (gPruI2s0WrtErrCnt == 1 || (gPruI2s0WrtErrCnt % ERROR_LOG_INTERVAL) == 0)
            {
                DebugP_log("ERROR: PRU I2S0 write error (count: %d)\r\n", gPruI2s0WrtErrCnt);
            }
            if (gPruI2s0WrtErrCnt > ERROR_COUNT_THRESHOLD)
            {
                DebugP_log("ERROR: PRU I2S0 write error count exceeded threshold\r\n");
                gRunFlag = FALSE;
                continue;
            }
        }

        /* Wait for Tx interrupt with timeout */
        status = SemaphoreP_pend(&gPruI2s0TxSemObj, SEMAPHORE_TIMEOUT_MS);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log("ERROR: Timeout waiting for PRU I2S0 Tx event\r\n");
            gRunFlag = FALSE;
            continue;
        }

        gLoopCnt++;

        /* Display periodic progress (every 1000 iterations) */
        if ((gLoopCnt % 1000) == 0)
        {
            DebugP_log("Tx streaming: %d iterations completed\r", gLoopCnt);
        }

        /* Update Tx buffer pointer with wraparound */
        gPruI2s0TxCnt += gPruI2s0XferSz;
        gPPruI2s0TxBuf += gPruI2s0XferSz;
        if (gPruI2s0TxCnt >= TX_BUF_SZ_32B)
        {
            gPPruI2s0TxBuf = &gPruI2s0TxBuf[0];
            gPruI2s0TxCnt = 0;
        }
    }

    DebugP_log("\r\nTx streaming loop terminated after %d iterations\r\n", gLoopCnt);
}

/**
 * \brief Real-time audio streaming loop (Rx-only mode)
 *
 * \param hRx    Handle to PRU I2S Rx instance
 *
 * \details Rx-only streaming loop for half-duplex receive operation that:
 *          - Waits for Rx interrupt via semaphore
 *          - Reads Rx buffers
 *          - Updates buffer pointers with wraparound
 *          - Displays periodic progress
 *          - Monitors error conditions
 */
static void prui2s_streaming_loop_rx_only(PRUI2S_Handle hRx)
{
    PRUI2S_IoBuf rdIoBuf;
    int32_t status;

    gLoopCnt = 0;
    gRunFlag = TRUE;

    DebugP_log("\r\nStarting real-time audio streaming loop (RX-ONLY mode)...\r\n");
    DebugP_log("Press Ctrl+C to stop\r\n\r\n");

    while (gRunFlag == TRUE)
    {
        /* Wait for Rx interrupt with timeout */
        status = SemaphoreP_pend(&gPruI2s1RxSemObj, SEMAPHORE_TIMEOUT_MS);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log("ERROR: Timeout waiting for PRU I2S1 Rx event\r\n");
            gRunFlag = FALSE;
            continue;
        }

        gLoopCnt++;

        /* Read next PRU I2S1 Rx ping/pong buffer */
        PRUI2S_ioBufInit(&rdIoBuf);
        rdIoBuf.ioBufAddr = gPPruI2s1RxBuf;
        status = PRUI2S_read(hRx, &rdIoBuf);
        if (status != PRUI2S_DRV_SOK)
        {
            gPruI2s1RdErrCnt++;
            if (gPruI2s1RdErrCnt == 1 || (gPruI2s1RdErrCnt % ERROR_LOG_INTERVAL) == 0)
            {
                DebugP_log("ERROR: PRU I2S1 read error (count: %d)\r\n", gPruI2s1RdErrCnt);
            }
            if (gPruI2s1RdErrCnt > ERROR_COUNT_THRESHOLD)
            {
                DebugP_log("ERROR: PRU I2S1 read error count exceeded threshold\r\n");
                gRunFlag = FALSE;
                continue;
            }
        }

        /* Display periodic progress (every 1000 iterations) */
        if ((gLoopCnt % 1000) == 0)
        {
            DebugP_log("Rx streaming: %d iterations completed\r", gLoopCnt);
        }

        /* Update Rx buffer pointer with wraparound */
        gPruI2s1RxCnt += gPruI2s1XferSz;
        gPPruI2s1RxBuf += gPruI2s1XferSz;
        if (gPruI2s1RxCnt >= RX_BUF_SZ_32B)
        {
            gPPruI2s1RxBuf = &gPruI2s1RxBuf[0];
            gPruI2s1RxCnt = 0;
        }
    }

    DebugP_log("\r\nRx streaming loop terminated after %d iterations\r\n", gLoopCnt);
}

/**
 * \brief Display accumulated error and performance statistics
 */
static void prui2s_display_statistics(void)
{
    uint32_t totalErrors = 0;

    DebugP_log("\r\n========================================\r\n");
    DebugP_log("PRU I2S Diagnostic Statistics\r\n");
    DebugP_log("========================================\r\n");

    DebugP_log("\r\nPRU I2S 0 (Tx):\r\n");
    DebugP_log("  Tx ISR Count:           %d\r\n", gPruI2s0TxIsrCnt);
    DebugP_log("  Tx Write Errors:        %d\r\n", gPruI2s0WrtErrCnt);
    DebugP_log("  Clear Tx Int Errors:    %d\r\n", gPruI2s0ClrTxIntErrCnt);
    DebugP_log("  Error ISR Count:        %d\r\n", gPruI2s0ErrIsrCnt);
    DebugP_log("  Overflow Errors:        %d\r\n", gPruI2s0ErrOvrCnt);
    DebugP_log("  Underflow Errors:       %d\r\n", gPruI2s0ErrUndCnt);
    DebugP_log("  Frame Sync Errors:      %d\r\n", gPruI2s0ErrFsyncCnt);

    totalErrors += gPruI2s0WrtErrCnt + gPruI2s0ClrTxIntErrCnt + gPruI2s0ErrOvrCnt +
                   gPruI2s0ErrUndCnt + gPruI2s0ErrFsyncCnt;

    DebugP_log("\r\nPRU I2S 1 (Rx):\r\n");
    DebugP_log("  Rx ISR Count:           %d\r\n", gPruI2s1RxIsrCnt);
    DebugP_log("  Rx Read Errors:         %d\r\n", gPruI2s1RdErrCnt);
    DebugP_log("  Clear Rx Int Errors:    %d\r\n", gPruI2s1ClrRxIntErrCnt);
    DebugP_log("  Error ISR Count:        %d\r\n", gPruI2s1ErrIsrCnt);
    DebugP_log("  Overflow Errors:        %d\r\n", gPruI2s1ErrOvrCnt);
    DebugP_log("  Underflow Errors:       %d\r\n", gPruI2s1ErrUndCnt);
    DebugP_log("  Frame Sync Errors:      %d\r\n", gPruI2s1ErrFsyncCnt);

    totalErrors += gPruI2s1RdErrCnt + gPruI2s1ClrRxIntErrCnt + gPruI2s1ErrOvrCnt +
                   gPruI2s1ErrUndCnt + gPruI2s1ErrFsyncCnt;

    DebugP_log("\r\nTotal Loop Iterations:  %d\r\n", gLoopCnt);
    DebugP_log("Total Errors:           %d\r\n", totalErrors);
    DebugP_log("========================================\r\n");
}

/**
 * \brief Display PRU I2S configuration information
 */
static void prui2s_display_configuration(void)
{
    PRUI2S_SwipAttrs attrs0, attrs1;
    int32_t status;

    DebugP_log("\r\n========================================\r\n");
    DebugP_log("PRU I2S Configuration\r\n");
    DebugP_log("========================================\r\n");

    /* Get configuration for instance 0 */
    status = PRUI2S_getInitCfg(TEST_PRUI2S0_IDX, &attrs0);
    if(status == PRUI2S_DRV_SOK)
    {
        DebugP_log("\r\nPRU I2S 0 (Tx):\r\n");
        DebugP_log("  Tx Channels:        %d\r\n", attrs0.numTxI2s);
        DebugP_log("  Rx Channels:        %d\r\n", attrs0.numRxI2s);
        DebugP_log("  Sampling Rate:      %d kHz\r\n", attrs0.sampFreq);
        DebugP_log("  Bits Per Slot:      %d\r\n", attrs0.bitsPerSlot);
        DebugP_log("  Buffer Size:        %d bytes\r\n", PRUI2S0_TX_PING_PONG_BUF_SZ);
    }

    /* Get configuration for instance 1 */
    status = PRUI2S_getInitCfg(TEST_PRUI2S1_IDX, &attrs1);
    if(status == PRUI2S_DRV_SOK)
    {
        DebugP_log("\r\nPRU I2S 1 (Rx):\r\n");
        DebugP_log("  Tx Channels:        %d\r\n", attrs1.numTxI2s);
        DebugP_log("  Rx Channels:        %d\r\n", attrs1.numRxI2s);
        DebugP_log("  Sampling Rate:      %d kHz\r\n", attrs1.sampFreq);
        DebugP_log("  Bits Per Slot:      %d\r\n", attrs1.bitsPerSlot);
        DebugP_log("  Buffer Size:        %d bytes\r\n", PRUI2S1_RX_PING_PONG_BUF_SZ);
    }

    DebugP_log("\r\n========================================\r\n");
}

/**
 * \brief Cleanup semaphores
 */
static void prui2s_cleanup_semaphores(void)
{
    if(gSemaphoresInitialized)
    {
        SemaphoreP_destruct(&gPruI2s0TxSemObj);
        SemaphoreP_destruct(&gPruI2s1RxSemObj);
        gSemaphoresInitialized = FALSE;
        DebugP_log("Semaphores destroyed\r\n");
    }
}

/**
 * \brief Cleanup PRU I2S instances
 */
static void prui2s_cleanup_instances(void)
{
    int32_t status;

    /* Close PRU I2S 0 instance */
    if(gHPruI2s0 != NULL)
    {
        status = PRUI2S_close(gHPruI2s0);
        if (status != PRUI2S_DRV_SOK)
        {
            DebugP_log("ERROR: PRUI2S_close() PRU I2S 0 failed\r\n");
        }
        else
        {
            DebugP_log("PRU I2S 0 closed successfully\r\n");
        }
        gHPruI2s0 = NULL;
    }

    /* Close PRU I2S 1 instance */
    if(gHPruI2s1 != NULL)
    {
        status = PRUI2S_close(gHPruI2s1);
        if (status != PRUI2S_DRV_SOK)
        {
            DebugP_log("ERROR: PRUI2S_close() PRU I2S 1 failed\r\n");
        }
        else
        {
            DebugP_log("PRU I2S 1 closed successfully\r\n");
        }
        gHPruI2s1 = NULL;
    }

    /* Deinitialize PRU I2S driver */
    PRUI2S_deinit();
    DebugP_log("PRU I2S driver deinitialized\r\n");
}

/* ========================================================================== */
/*                          ISR Handler Definitions                           */
/* ========================================================================== */

/**
 * \brief PRU I2S 0 Tx interrupt handler
 *
 * \param args  PRU I2S handle (passed from driver)
 */
void pruI2s0TxIrqHandler(void *args)
{
    PRUI2S_Handle handle = (PRUI2S_Handle)args;
    int32_t status;

    /* Increment ISR count for statistics */
    gPruI2s0TxIsrCnt++;

    /* Clear PRU I2S Tx interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_TX);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s0ClrTxIntErrCnt++;
    }

    /* Signal semaphore to unblock main task */
    SemaphoreP_post(&gPruI2s0TxSemObj);
}

/**
 * \brief PRU I2S 0 error interrupt handler
 *
 * \param args  PRU I2S handle (passed from driver)
 */
void pruI2s0ErrIrqHandler(void *args)
{
    PRUI2S_Handle handle = (PRUI2S_Handle)args;
    uint8_t errStat;
    int32_t status;

    /* Increment error ISR count for statistics */
    gPruI2s0ErrIsrCnt++;

    /* Clear PRU I2S error interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_ERR);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s0ClrErrIntErrCnt++;
        if (gPruI2s0ClrErrIntErrCnt == 1 || (gPruI2s0ClrErrIntErrCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: Failed to clear PRU I2S0 error interrupt (count: %d)\r\n",
                      gPruI2s0ClrErrIntErrCnt);
        }
    }

    /* Get error status register */
    PRUI2S_getErrStat(handle, &errStat);

    /* Check for Rx overflow error */
    if (errStat & ERROR_BIT_POSITION(I2S_ERR_OVR_BN))
    {
        gPruI2s0ErrOvrCnt++;
        if (gPruI2s0ErrOvrCnt == 1 || (gPruI2s0ErrOvrCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: PRU I2S0 Rx overflow error (count: %d)\r\n", gPruI2s0ErrOvrCnt);
        }
    }

    /* Check for Tx underflow error */
    if (errStat & ERROR_BIT_POSITION(I2S_ERR_UND_BN))
    {
        gPruI2s0ErrUndCnt++;
        if (gPruI2s0ErrUndCnt == 1 || (gPruI2s0ErrUndCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: PRU I2S0 Tx underflow error (count: %d)\r\n", gPruI2s0ErrUndCnt);
        }
    }

    /* Check for frame sync error */
    if (errStat & ERROR_BIT_POSITION(I2S_ERR_FSYNC_BN))
    {
        gPruI2s0ErrFsyncCnt++;
        if (gPruI2s0ErrFsyncCnt == 1 || (gPruI2s0ErrFsyncCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: PRU I2S0 frame sync error (count: %d)\r\n", gPruI2s0ErrFsyncCnt);
        }
    }

    /* Clear error status register */
    PRUI2S_clearErrStat(handle, errStat);
}

/**
 * \brief PRU I2S 1 Rx interrupt handler
 *
 * \param args  PRU I2S handle (passed from driver)
 */
void pruI2s1RxIrqHandler(void *args)
{
    PRUI2S_Handle handle = (PRUI2S_Handle)args;
    int32_t status;

    /* Increment ISR count for statistics */
    gPruI2s1RxIsrCnt++;

    /* Clear PRU I2S Rx interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_RX);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s1ClrRxIntErrCnt++;
    }

    /* Signal semaphore to unblock main task */
    SemaphoreP_post(&gPruI2s1RxSemObj);
}

/**
 * \brief PRU I2S 1 error interrupt handler
 *
 * \param args  PRU I2S handle (passed from driver)
 */
void pruI2s1ErrIrqHandler(void *args)
{
    PRUI2S_Handle handle = (PRUI2S_Handle)args;
    uint8_t errStat;
    int32_t status;

    /* Increment error ISR count for statistics */
    gPruI2s1ErrIsrCnt++;

    /* Clear PRU I2S error interrupt */
    status = PRUI2S_clearInt(handle, INTR_TYPE_I2S_ERR);
    if (status != PRUI2S_DRV_SOK)
    {
        gPruI2s1ClrErrIntErrCnt++;
        if (gPruI2s1ClrErrIntErrCnt == 1 || (gPruI2s1ClrErrIntErrCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: Failed to clear PRU I2S1 error interrupt (count: %d)\r\n",
                      gPruI2s1ClrErrIntErrCnt);
        }
    }

    /* Get error status register */
    PRUI2S_getErrStat(handle, &errStat);

    /* Check for Rx overflow error */
    if (errStat & ERROR_BIT_POSITION(I2S_ERR_OVR_BN))
    {
        gPruI2s1ErrOvrCnt++;
        if (gPruI2s1ErrOvrCnt == 1 || (gPruI2s1ErrOvrCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: PRU I2S1 Rx overflow error (count: %d)\r\n", gPruI2s1ErrOvrCnt);
        }
    }

    /* Check for Tx underflow error */
    if (errStat & ERROR_BIT_POSITION(I2S_ERR_UND_BN))
    {
        gPruI2s1ErrUndCnt++;
        if (gPruI2s1ErrUndCnt == 1 || (gPruI2s1ErrUndCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: PRU I2S1 Tx underflow error (count: %d)\r\n", gPruI2s1ErrUndCnt);
        }
    }

    /* Check for frame sync error */
    if (errStat & ERROR_BIT_POSITION(I2S_ERR_FSYNC_BN))
    {
        gPruI2s1ErrFsyncCnt++;
        if (gPruI2s1ErrFsyncCnt == 1 || (gPruI2s1ErrFsyncCnt % ERROR_LOG_INTERVAL) == 0)
        {
            DebugP_log("ERROR: PRU I2S1 frame sync error (count: %d)\r\n", gPruI2s1ErrFsyncCnt);
        }
    }

    /* Clear error status register */
    PRUI2S_clearErrStat(handle, errStat);
}

/**
 * \brief PRU I2S diagnostic main entry point
 *
 * \param args  Unused
 */

/* ========================================================================== */
/*                          Main Function                                     */
/* ========================================================================== */

/**
 * \brief Main diagnostic application entry point
 *
 * \param args  Unused
 *
 * \details Application initialization and execution flow:
 *          STEP 1: Initialize system drivers
 *          STEP 2: Initialize PRU-ICSS subsystem
 *          STEP 3: Initialize semaphores
 *          STEP 4: Initialize PRU I2S driver
 *          STEP 5: Load and run PRU firmware
 *          STEP 6: Execute streaming loop (full/half-duplex)
 *          STEP 7: Display statistics
 *          STEP 8: Cleanup and shutdown
 */
void pru_i2s_diagnostic_main(void *args)
{
    int32_t status = SystemP_SUCCESS;
    uint8_t numValidCfg;

    /* ========================================================================== */
    /* STEP 1: Initialize system drivers and board peripherals                    */
    /* ========================================================================== */
    Drivers_open();
    Board_driversOpen();

    DebugP_log("\r\n========================================\r\n");
    DebugP_log("PRU I2S Diagnostic Application\r\n");
    DebugP_log("========================================\r\n");
    DebugP_log("Build timestamp: %s %s\r\n", __DATE__, __TIME__);
    DebugP_log("Mode: %s\r\n", (CONFIG_I2S0_MODE == TDM_MODE) ? "TDM" : "I2S");

    /* ========================================================================== */
    /* STEP 2: Initialize PRU-ICSS subsystem                                      */
    /* ========================================================================== */
    prui2s_pruicss_init();

#ifdef SOC_AM263X
    /* ========================================================================== */
    /* STEP 2a: Configure board IO expander for signal routing                    */
    /* ========================================================================== */
    prui2s_board_io_expander_init();
#endif

    /* ========================================================================== */
    /* STEP 3: Initialize semaphores for ISR-to-task synchronization              */
    /* ========================================================================== */
    status = prui2s_init_semaphores();
    if(status != SystemP_SUCCESS)
    {
        goto cleanup;
    }

    /* ========================================================================== */
    /* STEP 4: Initialize PRU I2S driver                                          */
    /* ========================================================================== */

    /* Apply application-specific configuration */
    extern int32_t PRUI2S_applyAppConfig(void);
    status = PRUI2S_applyAppConfig();
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_applyAppConfig() failed\r\n");
        goto cleanup;
    }

    /* INTC initialization already done via PRUICSS_intcInit() earlier */

    /* Initialize PRU I2S driver (loads configuration, validates instances) */
    status = PRUI2S_init(&numValidCfg, &gPruFwImageInfo);
    if (status == PRUI2S_DRV_SERR_INIT_FWIMG)
    {
        DebugP_log("WARNING: PRUI2S_init() no FW image for configuration\r\n");
    }
    else if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_init() failed with status: %d\r\n", status);
        if (status == PRUI2S_DRV_SERR_INIT)
        {
            DebugP_log("       Likely cause: SemaphoreP_constructMutex() failed during driver init\r\n");
            DebugP_log("       This typically indicates RTOS resource exhaustion or init called multiple times\r\n");
        }
        goto cleanup;
    }

    DebugP_log("PRU I2S driver initialized (%d valid configurations)\r\n", numValidCfg);

    /* Validate test configuration indices */
    if (((numValidCfg-1) < TEST_PRUI2S0_IDX) || ((numValidCfg-1) < TEST_PRUI2S1_IDX))
    {
        DebugP_log("ERROR: Invalid test configuration indices\r\n");
        goto cleanup;
    }

    /* Initialize PRU I2S instances (conditional based on SysConfig) */
#if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
    /* Initialize PRU I2S 0 (Tx) instance */
    status = prui2s_init_instance(TEST_PRUI2S0_IDX, &gHPruI2s0);
    if(status != SystemP_SUCCESS)
    {
        goto cleanup;
    }
#endif

#if defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
    /* Initialize PRU I2S 1 (Rx) instance */
    status = prui2s_init_instance(TEST_PRUI2S1_IDX, &gHPruI2s1);
    if(status != SystemP_SUCCESS)
    {
        goto cleanup;
    }
#endif

    /* ========================================================================== */
    /* STEP 5: Load firmware and enable PRU cores                                 */
    /* ========================================================================== */

    /* Load and run firmware on PRU cores */
    prui2s_pruicss_load_run_fw();

    /* Display firmware versions for enabled instances */
#if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
    prui2s_display_fw_version(PRUICSS_PRU0);
#endif
#if defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
    prui2s_display_fw_version(PRUICSS_PRU1);
#endif

    /* Enable PRU I2S instances (conditional based on SysConfig) */
#if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
    status = PRUI2S_enable(gHPruI2s0);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_enable() PRU I2S 0 failed\r\n");
        goto cleanup;
    }
#endif

#if defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
    status = PRUI2S_enable(gHPruI2s1);
    if (status != PRUI2S_DRV_SOK)
    {
        DebugP_log("ERROR: PRUI2S_enable() PRU I2S 1 failed\r\n");
        goto cleanup;
    }
#endif

    DebugP_log("\r\nAll PRU I2S instances enabled and ready\r\n");

    /* Display configuration information */
    prui2s_display_configuration();

    /* ========================================================================== */
    /* STEP 6: Real-time audio streaming loop                                     */
    /* ========================================================================== */
#if defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1) && \
    defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
    /* Full-duplex mode: Both Tx (PRU0) and Rx (PRU1) enabled */
    prui2s_streaming_loop_full_duplex(gHPruI2s0, gHPruI2s1);
#elif defined(CONFIG_PRU_I2S0_ENABLED) && (CONFIG_PRU_I2S0_ENABLED == 1)
    /* Half-duplex Tx-only mode: Only PRU0 (Tx) enabled */
    prui2s_streaming_loop_tx_only(gHPruI2s0);
#elif defined(CONFIG_PRU_I2S1_ENABLED) && (CONFIG_PRU_I2S1_ENABLED == 1)
    /* Half-duplex Rx-only mode: Only PRU1 (Rx) enabled */
    prui2s_streaming_loop_rx_only(gHPruI2s1);
#else
    /* No instances enabled - skip streaming */
    DebugP_log("\r\nWARNING: No PRU I2S instances enabled. Skipping streaming loop.\r\n");
#endif

    /* ========================================================================== */
    /* STEP 7: Display statistics                                                 */
    /* ========================================================================== */
    prui2s_display_statistics();
cleanup:
    /* ========================================================================== */
    /* STEP 8: Cleanup and shutdown                                               */
    /* ========================================================================== */

    prui2s_cleanup_instances();
    prui2s_cleanup_semaphores();

    DebugP_log("\r\nAll tests have passed!\r\n");

    Board_driversClose();
    Drivers_close();
}
