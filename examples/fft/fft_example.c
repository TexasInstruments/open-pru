/*
 * Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/**
 *  \file   fft_example.c
 *
 *  \brief  FFT PRU-IO example implementation
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/pruicss.h>

/* Include PRU firmware */
#include <pru0_load_bin.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define FFT_SIZE 4096
#define FFT_OP_MEM_BASE 0x00014000
#define FFT_MAG_MEM_BASE 0x00010000
#define FFT_OP_MEM_SIZE (FFT_SIZE * 4)  /* 4 bytes per sample */
#define FFT_MAG_MEM_SIZE (FFT_SIZE * 2) /* 2 bytes per magnitude (N/2 + 1 values) */

/* Memory validation macros */
#define SHARED_MEM_START 0x00010000
#define SHARED_MEM_END   0x00020000
#define IS_VALID_MEM_RANGE(addr, size) \
    (((addr) >= SHARED_MEM_START) && (((addr) + (size)) <= SHARED_MEM_END))

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
static PRUICSS_Handle gPruIcssHandle = NULL;
static volatile uint32_t *gFftOutputPtr = NULL;      /* variable to access FFT output data in memory */
static volatile uint32_t *gMagnitudesPtr = NULL;     /* variable to access FFT magnitude data in memory */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
int32_t fft_post_processing(void);
int32_t fft_magnitude(int32_t real, int32_t img);
static int32_t validate_memory_access(void);
static void cleanup_resources(void);

/* ========================================================================== */
/*                          Function Definitions                             */
/* ========================================================================== */

void pru_io_fft_main(void *args)
{
    int32_t status;
    
    /* Open drivers to open the UART driver for console */
    Drivers_open();
    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);

    DebugP_log("FFT Example Started ...\r\n");

    /* Get PRUICSS handle */
    gPruIcssHandle = PRUICSS_open(CONFIG_PRU_ICSS0);
    DebugP_assert(gPruIcssHandle != NULL);

    /* Clear ICSS0 PRU0 data RAM */
    status = PRUICSS_initMemory(gPruIcssHandle, PRUICSS_DATARAM(PRUICSS_PRU0));
    DebugP_assert(status != 0);

    /* Load and run the PRU0 firmware */
    status = PRUICSS_loadFirmware(gPruIcssHandle, PRUICSS_PRU0, PRU0Firmware_0, sizeof(PRU0Firmware_0));
    DebugP_assert(SystemP_SUCCESS == status);

    DebugP_log("PRU firmware loaded successfully\r\n");
    DebugP_log("Note: Use the load_memory.js script to load input data and LUTs before running PRU\r\n");
    DebugP_log("Run the PRU0 firmware to perform FFT computation\r\n");

    /* Run the PRU0 firmware */
    status = PRUICSS_run(gPruIcssHandle, PRUICSS_PRU0);
    DebugP_assert(SystemP_SUCCESS == status);

    DebugP_log("FFT computation running on PRU0\r\n");
    DebugP_log("FFT output will be available at address 0x%08x\r\n", FFT_OP_MEM_BASE);

    /* Wait for PRU to complete (in a real application, you might check a status flag) */
    ClockP_sleep(1);

    /* Perform post-processing to calculate magnitudes */
    status = fft_post_processing();
    if (SystemP_SUCCESS != status) {
        DebugP_log("Error: FFT post-processing failed\r\n");
        goto cleanup;
    }

    DebugP_log("FFT magnitude calculation completed\r\n");
    DebugP_log("Magnitude values available at address 0x%08x\r\n", FFT_MAG_MEM_BASE);

cleanup:
    /* Clean up resources */
    cleanup_resources();
}

int32_t fft_post_processing(void)
{
    const int N = FFT_SIZE;
    int k;

    /* Validate memory ranges before accessing */
    if (!IS_VALID_MEM_RANGE(FFT_OP_MEM_BASE, FFT_OP_MEM_SIZE)) {
        DebugP_log("Error: FFT output memory range invalid\r\n");
        return SystemP_FAILURE;
    }
    
    if (!IS_VALID_MEM_RANGE(FFT_MAG_MEM_BASE, FFT_MAG_MEM_SIZE)) {
        DebugP_log("Error: FFT magnitude memory range invalid\r\n");
        return SystemP_FAILURE;
    }

    gFftOutputPtr    = (volatile uint32_t*)FFT_OP_MEM_BASE; /*point to the location of the FFT output data */
    gMagnitudesPtr = (volatile uint32_t*)FFT_MAG_MEM_BASE; /*point to the location where the magnitudes will be stored */

    /* Validate pointers are not NULL */
    if (gFftOutputPtr == NULL || gMagnitudesPtr == NULL) {
        DebugP_log("Error: Invalid memory pointers\r\n");
        return SystemP_FAILURE;
    }

    for (k = 1; k < N/2 ; k++)
    {
        int32_t real = (int32_t)gFftOutputPtr[k];
        int32_t img  = (int32_t)gFftOutputPtr[N - k];
        int32_t magnitude = fft_magnitude(real, img); /* Magnitude calculation in Q24 fixed-point */
        
        if (magnitude < 0) {
            DebugP_log("Warning: Negative magnitude calculated at index %d\r\n", k);
            magnitude = 0; /* Clamp to zero */
        }
        
        gMagnitudesPtr[k] = (uint32_t)magnitude;
    }
    gMagnitudesPtr[0]       = (uint32_t)abs((int32_t)gFftOutputPtr[0]);           /* DC component */
    gMagnitudesPtr[N/2]     = (uint32_t)abs((int32_t)gFftOutputPtr[N/2]);         /* Nyquist frequency */
    
    return SystemP_SUCCESS;
}

/**
 * int32_t fft_magnitude(int32_t real, int32_t img)
 *
 * \brief   This Function calculatees the magnitude value from real and imaginary components
 *
 * \param   real            real component
 * \param   img             imaginary component
 *
 * \return  magnitude_q24   magnitude value in q24 format
 */
int32_t fft_magnitude(int32_t real, int32_t img)
{
    /* Input validation - check for reasonable ranges to prevent overflow */
    const int32_t MAX_INPUT = (1LL << 23); /* Maximum safe input for Q24 format */
    
    if (abs(real) > MAX_INPUT || abs(img) > MAX_INPUT) {
        DebugP_log("Warning: Input values exceed safe range for magnitude calculation\r\n");
        /* Clamp inputs to safe range */
        real = (real > MAX_INPUT) ? MAX_INPUT : ((real < -MAX_INPUT) ? -MAX_INPUT : real);
        img = (img > MAX_INPUT) ? MAX_INPUT : ((img < -MAX_INPUT) ? -MAX_INPUT : img);
    }
    
    /* Cast to int64_t before multiplication to prevent overflow */
    int64_t real_sq      = (int64_t)real * (int64_t)real;               /* Real part squared */
    int64_t imag_sq      = (int64_t)img * (int64_t)img;                 /* Imaginary part squared */
    int64_t magnitude_sq = real_sq + imag_sq;                           /* Sum of squares */

    /* Check for overflow in sum */
    if (magnitude_sq < 0) {
        DebugP_log("Error: Magnitude calculation overflow detected\r\n");
        return 0; /* Return zero on overflow */
    }

    double magnitude_float = sqrt((double)magnitude_sq / (1LL << 48));  /* Scale down to floating-point */
    int32_t magnitude_q24 = (int32_t)(magnitude_float * (1 << 24));     /* Scale back to Q24 */

    /* Ensure result is non-negative */
    return (magnitude_q24 < 0) ? 0 : magnitude_q24;
}

/**
 * \brief   Validate memory access ranges
 *
 * \return  SystemP_SUCCESS on success, SystemP_FAILURE on error
 */
static int32_t validate_memory_access(void)
{
    /* Validate FFT output memory range */
    if (!IS_VALID_MEM_RANGE(FFT_OP_MEM_BASE, FFT_OP_MEM_SIZE)) {
        DebugP_log("Error: FFT output memory range [0x%08x - 0x%08x] is invalid\r\n", 
                   FFT_OP_MEM_BASE, FFT_OP_MEM_BASE + FFT_OP_MEM_SIZE);
        return SystemP_FAILURE;
    }
    
    /* Validate FFT magnitude memory range */
    if (!IS_VALID_MEM_RANGE(FFT_MAG_MEM_BASE, FFT_MAG_MEM_SIZE)) {
        DebugP_log("Error: FFT magnitude memory range [0x%08x - 0x%08x] is invalid\r\n", 
                   FFT_MAG_MEM_BASE, FFT_MAG_MEM_BASE + FFT_MAG_MEM_SIZE);
        return SystemP_FAILURE;
    }
    
    /* Check for memory overlap */
    if ((FFT_OP_MEM_BASE < FFT_MAG_MEM_BASE + FFT_MAG_MEM_SIZE) && 
        (FFT_MAG_MEM_BASE < FFT_OP_MEM_BASE + FFT_OP_MEM_SIZE)) {
        DebugP_log("Warning: FFT output and magnitude memory ranges overlap\r\n");
    }
    
    return SystemP_SUCCESS;
}

/**
 * \brief   Clean up allocated resources
 */
static void cleanup_resources(void)
{
    /* Close PRUICSS handle if it was opened */
    if (gPruIcssHandle != NULL) {
        PRUICSS_close(gPruIcssHandle);
        gPruIcssHandle = NULL;
    }
    
    /* Close board drivers */
    Board_driversClose();
    
    /* Close system drivers */
    Drivers_close();
    
    DebugP_log("Resources cleaned up successfully\r\n");
}