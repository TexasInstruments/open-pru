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

#ifndef _PRU_I2S_APP_CONFIG_H_
#define _PRU_I2S_APP_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ========================================================================== */
/*                    I2S MODE CONFIGURATION CONSTANTS                       */
/* ========================================================================== */

#define I2S_MODE                   0       /**< Standard I2S mode */
#define TDM_MODE                   1       /**< Time Division Multiplexing mode */
#define CONFIG_I2S0_MODE           TDM_MODE /**< Selected mode: TDM mode=1, I2S mode=0 */

/* ========================================================================== */
/*              SYSCONFIG-BASED INSTANCE ENABLE FLAGS                        */
/* ========================================================================== */

#ifndef CONFIG_PRU_I2S0_ENABLED
#define CONFIG_PRU_I2S0_ENABLED    1       /**< Enable PRU I2S 0 (Tx on PRU0) */
#endif

#ifndef CONFIG_PRU_I2S1_ENABLED
#define CONFIG_PRU_I2S1_ENABLED    1       /**< Enable PRU I2S 1 (Rx on PRU1) */
#endif

/* ========================================================================== */
/*                    PRU I2S INSTANCE INDICES                               */
/* ========================================================================== */

#define PRU_I2S0_INSTANCE_IDX      0       /**< Instance index for PRU I2S 0 (Tx only) */
#define PRU_I2S1_INSTANCE_IDX      1       /**< Instance index for PRU I2S 1 (Rx only) */

/* ========================================================================== */
/*              TEST INSTANCE INDICES - PRU I2S INSTANCES USED               */
/* ========================================================================== */

#define TEST_PRUI2S0_IDX           PRU_I2S0_INSTANCE_IDX  /**< Test PRU I2S 0 index (for Tx) */
#define TEST_PRUI2S1_IDX           PRU_I2S1_INSTANCE_IDX  /**< Test PRU I2S 1 index (for Rx) */

/* ========================================================================== */
/*                          FUNCTION DECLARATIONS                            */
/* ========================================================================== */

/**
 * \brief Apply PRU I2S application configuration
 *
 * This function applies the user configuration to the PRU I2S driver.
 * It must be called before PRUI2S_init().
 *
 * \return 0 on success, negative error code on failure
 */
int32_t PRUI2S_applyAppConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* _PRU_I2S_APP_CONFIG_H_ */
