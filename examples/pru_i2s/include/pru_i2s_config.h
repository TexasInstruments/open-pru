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
 *  \file pru_i2s_config.h
 *
 *  \brief PRU I2S Configuration Header
 *
 *  This file contains user-editable configuration parameters for PRU I2S driver.
 *  Applications can modify this file to configure:
 *  - ICSS instance (ICSSM0 or ICSSM1)
 *  - PRU core selection (PRU0 or PRU1)
 *  - I2S/TDM mode
 *  - Number of Tx/Rx channels
 *  - Bits per slot
 *  - Sampling frequency
 */

#ifndef PRU_I2S_CONFIG_H_
#define PRU_I2S_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <drivers/pruicss.h>

/* =========================== Configuration Section =========================== */

/**
 * \brief PRU I2S Configuration Structure
 *
 * This structure defines the configuration for a PRU I2S instance.
 * Applications should create instances of this structure to configure each PRU core.
 */
typedef struct {
    /* ICSS and PRU Core Selection */
    uint8_t  icssInstId;        /**< ICSS instance: 0=ICSSM0, 1=ICSSM1 */
    uint8_t  pruInstId;         /**< PRU core: PRUICSS_PRU0 or PRUICSS_PRU1 */

    /* I2S/TDM Configuration (optional - can be auto-detected from firmware) */
    uint8_t  numTxI2s;          /**< Number of Tx I2S channels (0 = auto-detect from firmware) */
    uint8_t  numRxI2s;          /**< Number of Rx I2S channels (0 = auto-detect from firmware) */
    uint8_t  sampFreq;          /**< Sampling frequency in kHz (0 = auto-detect from firmware) */
    uint8_t  bitsPerSlot;       /**< Bits per I2S slot (0 = auto-detect from firmware) */

} PRUI2S_UserConfig;

/* =========================== Example Configurations =========================== */

/*
 * Example 1: AM261x LaunchPad - ICSSM1 PRU0 for Tx
 * - Uses ICSSM1 (icssInstId = 1)
 * - Uses PRU0 (pruInstId = PRUICSS_PRU0)
 * - Auto-detects I2S parameters from firmware
 */
#define PRUI2S_CONFIG_AM261X_LP_PRU0  \
{                                      \
    .icssInstId = 1,                   \
    .pruInstId = PRUICSS_PRU0,         \
    .numTxI2s = 0,                     \
    .numRxI2s = 0,                     \
    .sampFreq = 0,                     \
    .bitsPerSlot = 0,                  \
}

/*
 * Example 2: AM261x LaunchPad - ICSSM1 PRU1 for Rx
 * - Uses ICSSM1 (icssInstId = 1)
 * - Uses PRU1 (pruInstId = PRUICSS_PRU1)
 * - Auto-detects I2S parameters from firmware
 */
#define PRUI2S_CONFIG_AM261X_LP_PRU1  \
{                                      \
    .icssInstId = 1,                   \
    .pruInstId = PRUICSS_PRU1,         \
    .numTxI2s = 0,                     \
    .numRxI2s = 0,                     \
    .sampFreq = 0,                     \
    .bitsPerSlot = 0,                  \
}

/*
 * Example 3: AM263x Control Card - ICSSM0 PRU0 for Tx and Rx
 * - Uses ICSSM0 (icssInstId = 0)
 * - Uses PRU0 (pruInstId = PRUICSS_PRU0)
 * - Auto-detects I2S parameters from firmware
 */
#define PRUI2S_CONFIG_AM263X_CC_PRU0  \
{                                      \
    .icssInstId = 0,                   \
    .pruInstId = PRUICSS_PRU0,         \
    .numTxI2s = 0,                     \
    .numRxI2s = 0,                     \
    .sampFreq = 0,                     \
    .bitsPerSlot = 0,                  \
}

/*
 * Example 4: AM263x Control Card - ICSSM0 PRU1 for Tx and Rx
 * - Uses ICSSM0 (icssInstId = 0)
 * - Uses PRU1 (pruInstId = PRUICSS_PRU1)
 * - Auto-detects I2S parameters from firmware
 */
#define PRUI2S_CONFIG_AM263X_CC_PRU1  \
{                                      \
    .icssInstId = 0,                   \
    .pruInstId = PRUICSS_PRU1,         \
    .numTxI2s = 0,                     \
    .numRxI2s = 0,                     \
    .sampFreq = 0,                     \
    .bitsPerSlot = 0,                  \
}

/* =========================== Default Configuration Selection =========================== */

/*
 * Define the default configuration for your application.
 * Uncomment and modify based on your hardware platform.
 */
#ifdef SOC_AM261X
    /* AM261x uses ICSSM1 */
    #define PRUI2S_USER_CONFIG_PRU0  PRUI2S_CONFIG_AM261X_LP_PRU0
    #define PRUI2S_USER_CONFIG_PRU1  PRUI2S_CONFIG_AM261X_LP_PRU1
#else
    /* AM263x uses ICSSM0 */
    #define PRUI2S_USER_CONFIG_PRU0  PRUI2S_CONFIG_AM263X_CC_PRU0
    #define PRUI2S_USER_CONFIG_PRU1  PRUI2S_CONFIG_AM263X_CC_PRU1
#endif

/* =========================== Function Prototypes =========================== */

/**
 * \brief Initialize PRU I2S driver with user configuration
 *
 * This function should be called by the application to initialize the PRU I2S driver
 * with the user-specified configuration. It must be called before PRUI2S_init().
 *
 * NOTE: The actual PRUI2S_setUserConfig() function is declared in pru_i2s_drv.h
 * This header provides the PRUI2S_UserConfig structure definition and example configurations.
 */

#ifdef __cplusplus
}
#endif

#endif /* PRU_I2S_CONFIG_H_ */
