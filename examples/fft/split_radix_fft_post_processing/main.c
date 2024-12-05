/*
 * Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
 *
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
 *  \file   main.c
 *
 *  \brief  File containing function for FFT magnitude calculation from split radix FFT output
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <math.h>
#include <stdint.h>
/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define FFT_SIZE 4096
#define FFT_OP_MEM 0x00014000
#define FFT_MAG_MEM 0x00010000

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
volatile uint32_t *fft_out;      /* variable to access FFT output data in memory */
volatile uint32_t *magnitudes;   /* variable to access FFT output data in memory */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
int32_t fft_magnitude(int32_t real, int32_t img);

/*
 * void main(void)
 */
void main(void)
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

    __halt();

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


