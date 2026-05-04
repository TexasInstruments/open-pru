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
 *  \file pru_i2s_app_config.c
 *
 *  \brief PRU I2S Application Configuration
 *
 *  This file contains the application-specific configuration for PRU I2S.
 *  Modify pru_i2s_app_config.h to configure:
 *  - ICSS instance selection (ICSSM0 or ICSSM1)
 *  - PRU core selection (PRU0 or PRU1)
 *  - I2S/TDM mode selection
 *  - Instance enable flags
 */

#include "pru_i2s_config.h"
#include "pru_i2s_drv.h"
#include "pru_i2s_app_config.h"

/* ========================================================================== */
/*                          USER CONFIGURATION                                 */
/* ========================================================================== */

/**
 * \brief PRU I2S Configuration Table
 *
 * This table defines the configuration for each PRU I2S instance.
 *
 * For AM261x LaunchPad:
 *   - Uses ICSSM1 (icssInstId = 1)
 *   - PRU0 for Tx, PRU1 for Rx
 *
 * For AM263x Control Card:
 *   - Uses ICSSM0 (icssInstId = 0)
 *   - PRU0 and PRU1 for Tx and Rx
 *
 * Modify the values below to match your hardware configuration.
 */
static const PRUI2S_UserConfig gPruI2sAppConfig[PRU_I2S_NUM_CONFIG] =
{
    /* Configuration 0: First PRU core */
    PRUI2S_USER_CONFIG_PRU0,

    /* Configuration 1: Second PRU core */
    PRUI2S_USER_CONFIG_PRU1
};

/* ========================================================================== */
/*                          FUNCTION DEFINITIONS                               */
/* ========================================================================== */

/**
 * \brief Apply PRU I2S application configuration
 *
 * This function applies the user configuration to the PRU I2S driver.
 * It must be called before PRUI2S_init().
 *
 * \return 0 on success, negative error code on failure
 */
int32_t PRUI2S_applyAppConfig(void)
{
    int32_t status = PRUI2S_DRV_SOK;
    uint8_t i;

    for (i = 0; i < PRU_I2S_NUM_CONFIG; i++)
    {
        status = PRUI2S_setUserConfig(
            i,
            gPruI2sAppConfig[i].icssInstId,
            gPruI2sAppConfig[i].pruInstId
        );

        if (status != PRUI2S_DRV_SOK)
        {
            break;
        }
    }

    return status;
}
