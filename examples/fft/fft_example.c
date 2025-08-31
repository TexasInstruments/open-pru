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
#define FFT_OP_MEM 0x00014000
#define FFT_MAG_MEM 0x00010000

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
static PRUICSS_Handle gPruIcssHandle;
volatile uint32_t *fft_out;      /* variable to access FFT output data in memory */
volatile uint32_t *magnitudes;   /* variable to access FFT output data in memory */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
int32_t fft_magnitude(int32_t real, int32_t img);

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
    DebugP_log("FFT output will be available at address 0x%08x\r\n", FFT_OP_MEM);

    /* Wait for PRU to complete (in a real application, you might check a status flag) */
    ClockP_sleep(1);

    /* Perform post-processing to calculate magnitudes */
    fft_post_processing();

    DebugP_log("FFT magnitude calculation completed\r\n");
    DebugP_log("Magnitude values available at address 0x%08x\r\n", FFT_MAG_MEM);

    /* Clean up resources */
    PRUICSS_close(gPruIcssHandle);
    Board_driversClose();
    Drivers_close();
}

void fft_post_processing(void)
{
    const int N = FFT_SIZE;
    int k;

    fft_out    = (uint32_t*)FFT_OP_MEM; /*point to the location of the FFT output data */
    magnitudes = (uint32_t*)FFT_MAG_MEM; /*point to the location where the magnitudes will be stored */

    for (k = 1; k < N/2 ; k++)
    {
        int32_t real = fft_out[k];
        int32_t img  = fft_out[N - k];
        magnitudes[k] = fft_magnitude(real, img); /* Magnitude calculation in Q24 fixed-point */
    }
    magnitudes[0]       = abs(fft_out[0]);           /* DC component */
    magnitudes[N/2]     = abs(fft_out[N/2]);         /* Nyquist frequency */
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
    int64_t real_sq      = (int64_t)real * real;                        /* Real part squared */
    int64_t imag_sq      = (int64_t)img * img;                          /* Imaginary part squared */
    int64_t magnitude_sq = real_sq + imag_sq;                           /* Sum of squares */

    double magnitude_float = sqrt((double)magnitude_sq / (1LL << 48));  /* Scale down to floating-point */
    int32_t magnitude_q24 = (int32_t)(magnitude_float * (1 << 24));     /* Scale back to Q24 */

    return magnitude_q24;
}