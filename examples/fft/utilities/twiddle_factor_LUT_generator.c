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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define FFT_SIZE 4096 
#define TWO_PI 6.2831853071719586f      /* (2*pi) */
#define TWO_RAISED_TO_32 4294967296.0   /* (2^32) */
#define TWO_RAISED_TO_24 16777216.0     /* (2^24) */

void rfft_tw_fac_gen(int N); 

int main(void)
{
    rfft_tw_fac_gen(FFT_SIZE); /*generate twiddle factors for configured size of FFT*/
    return 0;
}


/***************************************************************************
* void rfft_tw_fac_gen(int N)                                              *
*     Generates twiddle factors for real valued split radix fft            *
*     for given size N and saves output to a dat file                      *
****************************************************************************/
void rfft_tw_fac_gen(int N)
{
    int J,K,M,N2,N4,N8;
    double A,A3,CC1,SS1,CC3,SS3,E;
    unsigned int q32_val;
    int32_t q24_val;

    M  = (int)(round(log(N)/log(2.0))); /* N=2^M */
    N2 = 2;

    /*Save to a .dat file in binary format*/ 
    FILE *file = fopen("twiddle_factor_lut.dat", "wb");

    for(K=2;K<=M;K++)
        {
            N2    = N2 * 2;
            N8    = N2/8;
            E     = (double) TWO_PI/N2;
            A = E;
            for(J= 2;J<=N8;J++)
                    {       
                    A3    = 3.0 * A;
                    CC1   = cos(A);
                    q32_val = (unsigned int)(CC1 * TWO_RAISED_TO_32);
                    fwrite(&q32_val, sizeof(unsigned int), 1, file);

                    SS1   = sin(A); 
                    q32_val = (unsigned int)(SS1 * TWO_RAISED_TO_32);
                    fwrite(&q32_val, sizeof(unsigned int), 1, file);

                    CC3   = cos(A3); 
                    q24_val = (int32_t)(CC3 * TWO_RAISED_TO_24);
                    fwrite(&q24_val, sizeof(int32_t), 1, file);

                    SS3   = sin(A3);
                    q32_val = (unsigned int)(SS3 * TWO_RAISED_TO_32);
                    fwrite(&q32_val, sizeof(unsigned int), 1, file);

                    A = (float)J * E;

                    }
            }

    /*Close the file after storing all values*/ 
    fclose(file);

    printf("Twiddle factors stored to \'twiddle_factor_lut.dat\' ");

    return;
}
