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
#ifndef DATA_H_
#define DATA_H_
/* Constants for system configuration and behavior */
#define SEMAPHORE_TIMEOUT_MS       5000    /* 5 second timeout for semaphore operations */
#define ERROR_COUNT_THRESHOLD      10      /* Threshold for error counts before taking action */
#define ERROR_LOG_INTERVAL         10      /* Log errors on first occurrence and every Nth after */

/* Constants for buffer management */
#define PING_PONG_BUFFER_COUNT     2       /* Number of ping-pong buffers (always 2) */
#define RX_BUFFER_STORAGE_FACTOR   60      /* Store data equivalent to 60 ping-pong buffers */
#define BYTES_PER_32BIT_WORD       4       /* Number of bytes in a 32-bit word */

/* IO Expander index values */
#define IOEXP_INDEX_P20            0x10    /* P20 = LED 3 bits, pin, 2 bits port */
#define IOEXP_INDEX_P02            0x02    /* P02 index */
#define IOEXP_INDEX_P03            0x03    /* P03 index */

/* Audio processing constants */
#ifdef _DBG_PRUI2S_RX_TO_TX_LB
#define GAIN_MINUS_3DB             0.7071f /* Left channel gain (-3 dB attenuation) */
#define GAIN_MINUS_6DB             0.5f    /* Right channel gain (-6 dB attenuation) */
#define CHANNEL_MIX_DIVISOR        2       /* Divisor for channel mixing (simple average) */
#endif

/* Error bit position macro */
#define ERROR_BIT_POSITION(bit)    (1 << (bit))
#define BYTES_PER_32BIT_WORD 4
/* Rx buffers -- interleaved format */
/* Redefine with more descriptive constants */
#define RX_BUF_SZ       (PRUI2S1_RX_PING_PONG_BUF_SZ/PING_PONG_BUFFER_COUNT*RX_BUFFER_STORAGE_FACTOR)  /* 60 PP buffers */
#define RX_BUF_SZ_32B   (RX_BUF_SZ/BYTES_PER_32BIT_WORD)

/* Tx ping/pong buffer addresses in ICSSG SHMEM (local PRU view, not SoC view) */
#define PRUI2S0_TX_PING_PONG_BASE_ADDR  ( 0x10000 )

/* PRU I2S0 total Tx ping+pong buffer size in bytes
    32 sample/channel * (4 byte/sample) * (2 channel/I2S) * (3 I2S/PP buffer) = 768 byte/PP buffer
    768 byte/PP buffer * 2 PP buffer = 1536 bytes
*/
#define PRUI2S0_TX_PING_PONG_BUF_SZ     ( 1536 )
/* PRU I2S0 total Tx ping+pong buffer size in 32-bit words */
#define PRUI2S0_TX_PING_PONG_BUF_SZ_32B ( PRUI2S0_TX_PING_PONG_BUF_SZ/4 )

/* PRU I2S1 total Rx ping+pong buffer size in bytes
    32 sample/channel * (4 byte/sample) * (2 channel/I2S) * (2 I2S/PP buffer) = 512 byte/PP buffer
    512 byte/PP buffer * 2 PP buffer = 1024 bytes
*/
#define PRUI2S1_RX_PING_PONG_BUF_SZ     ( 1536 )
/* PRU I2S1 total Rx ping+pong buffer size in 32-bit words */
#define PRUI2S1_RX_PING_PONG_BUF_SZ_32B ( PRUI2S1_RX_PING_PONG_BUF_SZ/4 )

/* Rx ping/pong buffers */
__attribute__((section(".PruI2s1RxPingPongBuf"))) int32_t gPruI2s1RxPingPongBuf[PRUI2S1_RX_PING_PONG_BUF_SZ_32B];

/* Tx buffers -- interleaved format */
#define TX_BUF_SZ       ( PRUI2S0_TX_PING_PONG_BUF_SZ/2*60 )    /* 60 PP buffers */
#define TX_BUF_SZ_32B   ( TX_BUF_SZ/4 )
__attribute__((section(".TxBuf"))) int32_t gPruI2s0TxBuf[TX_BUF_SZ_32B] =
{0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0001,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,
 0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000,0xaaaaffff,0x0000aaaa,0x0000ffff,0xaaaa0000
 };
#endif /* DATA_H_ */
