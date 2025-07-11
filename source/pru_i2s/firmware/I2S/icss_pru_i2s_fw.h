/*
 * Copyright (c) 2025, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ICSS_pru_i2s_FW_H_
#define _ICSS_pru_i2s_FW_H_

/* 
    Firmware registers 
*/

/* FW register base address */
#define ICSS_PRUI2S_FW_REG_BASE                     ( 0x0000 )

/* FW register sizes (in bytes) */
#define FW_REG_NUM_TX_I2S_SZ                        ( 1 )
#define FW_REG_NUM_RX_I2S_SZ                        ( 1 )
#define FW_REG_SAMP_FREQ_SZ                         ( 1 )
#define FW_REG_BITS_PER_SLOT_SZ                     ( 1 )
#define FW_REG_TX_PING_PONG_BUF_ADDR_SZ             ( 4 )
#define FW_REG_PING_PONG_BUF_SZ_SZ                  ( 2 )
#define FW_REG_I2S_ICSS_INTC_SYS_EVT_SZ             ( 1 )
#define FW_REG_I2S_ERR_ICSS_INTC_SYS_EVT_SZ         ( 1 )
#define FW_REG_I2S_PIN_NUM_SZ                       ( 1 )

/* FW register offsets from base (in bytes) */
#define FW_REG_NUM_TX_I2S_OFFSET                    ( 0x00 )
#define FW_REG_NUM_RX_I2S_OFFSET                    ( 0x01 )
#define FW_REG_SAMP_FREQ_OFFSET                     ( 0x02 )
#define FW_REG_BITS_PER_SLOT_OFFSET                 ( 0x03 )
#define FW_REG_TX_PING_PONG_BUF_ADDR_OFFSET         ( 0x04 )
#define FW_REG_PING_PONG_BUF_SZ_OFFSET              ( 0x08 )
#define FW_REG_I2S_TX_ICSS_INTC_SYS_EVT_OFFSET      ( 0x0A )
#define FW_REG_I2S_RX_ICSS_INTC_SYS_EVT_OFFSET      ( 0x0B )
#define FW_REG_I2S_ERR_ICSS_INTC_SYS_EVT_OFFSET     ( 0x0C )
#define FW_REG_I2S_PIN_NUM_BCLK_OFFSET              ( 0x0D )
#define FW_REG_I2S_PIN_NUM_FSYNC_OFFSET             ( 0x0E )
#define FW_REG_I2S_PIN_NUM_TX0_OFFSET               ( 0x0F )
#define FW_REG_I2S_PIN_NUM_TX1_OFFSET               ( 0x10 )
#define FW_REG_I2S_PIN_NUM_TX2_OFFSET               ( 0x11 )
#define FW_REG_I2S_PIN_NUM_RX0_OFFSET               ( 0x12 )
#define FW_REG_I2S_PIN_NUM_RX1_OFFSET               ( 0x13 )
#define FW_REG_RX_PING_PONG_BUF_ADDR_OFFSET         ( 0x14 )
#define FW_REG_TX_PING_PONG_SEL_OFFSET              ( 0x18 )
#define FW_REG_RX_PING_PONG_SEL_OFFSET              ( 0x19 )
#define FW_REG_TX_PING_PONG_STAT_OFFSET             ( 0x1A )
#define FW_REG_RX_PING_PONG_STAT_OFFSET             ( 0x1B )
#define FW_REG_ERR_STAT_OFFSET                      ( 0x1C )

/* FW register addresses */
#define FW_REG_NUM_TX_I2S                           ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_NUM_TX_I2S_OFFSET )
#define FW_REG_NUM_RX_I2S                           ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_NUM_RX_I2S_OFFSET )
#define FW_REG_SAMP_FREQ                            ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_SAMP_FREQ_OFFSET )
#define FW_REG_BITS_PER_SLOT                        ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_BITS_PER_SLOT_OFFSET )
#define FW_REG_TX_PING_PONG_BUF_ADDR                ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_TX_PING_PONG_BUF_ADDR_OFFSET )
#define FW_REG_PING_PONG_BUF_SZ                     ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_PING_PONG_BUF_SZ_OFFSET )
#define FW_REG_I2S_TX_ICSS_INTC_SYS_EVT             ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_TX_ICSS_INTC_SYS_EVT_OFFSET )
#define FW_REG_I2S_RX_ICSS_INTC_SYS_EVT             ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_RX_ICSS_INTC_SYS_EVT_OFFSET )
#define FW_REG_I2S_ERR_ICSS_INTC_SYS_EVT            ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_ERR_ICSS_INTC_SYS_EVT_OFFSET )
#define FW_REG_I2S_PIN_NUM_BCLK                     ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_BCLK_OFFSET )
#define FW_REG_I2S_PIN_NUM_FSYNC                    ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_FSYNC_OFFSET )
#define FW_REG_I2S_PIN_NUM_TX0                      ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_TX0_OFFSET )
#define FW_REG_I2S_PIN_NUM_TX1                      ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_TX1_OFFSET )
#define FW_REG_I2S_PIN_NUM_TX2                      ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_TX2_OFFSET )
#define FW_REG_I2S_PIN_NUM_RX0                      ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_RX0_OFFSET )
#define FW_REG_I2S_PIN_NUM_RX1                      ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_I2S_PIN_NUM_RX1_OFFSET )
#define FW_REG_RX_PING_PONG_BUF_ADDR                ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_RX_PING_PONG_BUF_ADDR_OFFSET )
#define FW_REG_TX_PING_PONG_SEL                     ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_TX_PING_PONG_SEL_OFFSET )
#define FW_REG_RX_PING_PONG_SEL                     ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_RX_PING_PONG_SEL_OFFSET )
#define FW_REG_TX_PING_PONG_STAT                    ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_TX_PING_PONG_STAT_OFFSET )
#define FW_REG_RX_PING_PONG_STAT                    ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_RX_PING_PONG_STAT_OFFSET )
#define FW_REG_ERR_STAT                             ( ICSS_PRUI2S_FW_REG_BASE + FW_REG_ERR_STAT_OFFSET )

/*
    Firmware register bit fields 
*/

/* NUM_TX_I2S */
#define BF_NUM_TX_I2S_MASK                          ( 0x3 )
#define NUM_TX_I2S_BF_NUM_TX_I2S_SHIFT              ( 0 )
#define NUM_TX_I2S_BF_NUM_TX_I2S_MASK               ( BF_NUM_TX_I2S_MASK << NUM_TX_I2S_BF_NUM_TX_I2S_SHIFT )

/* NUM_RX_I2S */
#define BF_NUM_RX_I2S_MASK                          ( 0x3 )
#define NUM_TX_I2S_BF_NUM_RX_I2S_SHIFT              ( 0 )
#define NUM_TX_I2S_BF_NUM_RX_I2S_MASK               ( BF_NUM_RX_I2S_MASK << NUM_TX_I2S_BF_NUM_RX_I2S_SHIFT )

/* FW_REG_SAMP_FREQ */
#define BF_SAMP_FREQ_MASK                           ( 0xFF )
#define SAMP_FREQ_BF_SAMP_FREQ_SHIFT                ( 0 )
#define SAMP_FREQ_BF_SAMP_FREQ_MASK                 ( BF_SAMP_FREQ_MASK << SAMP_FREQ_BF_SAMP_FREQ_SHIFT )

/* FW_REG_BITS_PER_SLOT */
#define BF_BITS_PER_SLOT_MASK                       ( 0x1F )
#define BITS_PER_SLOT_BF_BITS_PER_SLOT_SHIFT        ( 0 )
#define BITS_PER_SLOT_BF_BITS_PER_SLOT_MASK         ( BF_BITS_PER_SLOT_MASK << BITS_PER_SLOT_BF_BITS_PER_SLOT_SHIFT )

/* FW_REG_TX_PING_PONG_BUF_ADDR */
#define BF_TX_BUF_PING_PONG_BUF_ADDR_MASK           ( 0xFFFFFFFF )
#define TX_BUF_PING_PONG_BUF_ADDR_BF_TX_BUF_PING_PONG_BUF_ADDR_SHIFT \
    ( 0 )
#define TX_BUF_PING_PONG_BUF_ADDR_BF_TX_BUF_PING_PONG_BUF_ADDR_MASK \
    ( BF_TX_BUF_PING_PONG_BUF_ADDR_MASK << TX_BUF_PING_PONG_BUF_ADDR_BF_TX_BUF_PING_PONG_BUF_ADDR_SHIFT )

/* FW_REG_PING_PONG_BUF_SZ */
#define BF_PING_PONG_BUF_SZ_MASK                    ( 0xFFFF )
#define TX_PING_PONG_BUF_SZ_BF_PING_PONG_BUF_SZ_SHIFT \
    ( 0 )
#define TX_PING_PONG_BUF_SZ_BF_PING_PONG_BUF_SZ_MASK \
    ( BF_PING_PONG_BUF_SZ_MASK << TX_PING_PONG_BUF_SZ_BF_PING_PONG_BUF_SZ_SHIFT )

/* FW_REG_I2S_ICSS_INTC_SYS_EVT */
#define BF_I2S_ICSS_INTC_SYS_EVT_MASK               ( 0xFF )
#define I2S_ICSS_INTC_SYS_EVT_BF_I2S_ICSS_INTC_SYS_EVT_SHIFT \
    ( 0 )
#define I2S_ICSS_INTC_SYS_EVT_BF_I2S_ICSS_INTC_SYS_EVT_MASK \
    ( BF_I2S_ICSS_INTC_SYS_EVT_MASK << I2S_ICSS_INTC_SYS_EVT_BF_I2S_ICSS_INTC_SYS_EVT_SHIFT )

/* FW_REG_I2S_ERR_ICSS_INTC_SYS_EVT */
#define BF_I2S_ERR_ICSS_INTC_SYS_EVT_MASK           ( 0xFF )
#define I2S_ERR_ICSS_INTC_SYS_EVT_BF_I2S_ERR_ICSS_INTC_SYS_EVT_SHIFT \
    ( 0 )
#define I2S_ERR_ICSS_INTC_SYS_EVT_BF_I2S_ERR_ICSS_INTC_SYS_EVT_MASK \
    ( BF_I2S_ERR_ICSS_INTC_SYS_EVT_MASK << I2S_ERR_ICSS_INTC_SYS_EVT_BF_I2S_ERR_ICSS_INTC_SYS_EVT_SHIFT )

/* FW_REG_I2S_PIN_NUM_<x> */
#define BF_I2S_PIN_NUM_MASK                         ( 0xFF )
#define I2S_PIN_NUM_BF_I2S_PIN_NUM_SHIFT            ( 0 )
#define I2S_PIN_NUM_BF_I2S_PIN_NUM_MASK             ( BF_I2S_PIN_NUM_MASK << I2S_PIN_NUM_BF_I2S_PIN_NUM_SHIFT )

/* FW_REG_TX_PING_PONG_SEL */
#define BF_TX_PING_PONG_SEL_MASK                    ( 0x1 )
#define TX_PING_PONG_SEL_BF_TX_PING_PONG_SEL_SHIFT  ( 0 )
#define TX_PING_PONG_SEL_BF_TX_PING_PONG_SEL_MASK   ( BF_TX_PING_PONG_SEL_MASK << TX_PING_PONG_SEL_BF_TX_PING_PONG_SEL_SHIFT)

/* FW_REG_RX_PING_PONG_SEL */
#define BF_RX_PING_PONG_SEL_MASK                    ( 0x1 )
#define RX_PING_PONG_SEL_BF_RX_PING_PONG_SEL_SHIFT  ( 0 )
#define RX_PING_PONG_SEL_BF_RX_PING_PONG_SEL_MASK   ( BF_RX_PING_PONG_SEL_MASK << RX_PING_PONG_SEL_BF_RX_PING_PONG_SEL_SHIFT)

/* FW_REG_TX_PING_PONG_STAT */
#define BF_TX_PING_PONG_STAT_MASK                   ( 0x1 )
#define TX_PING_PONG_STAT_BF_TX_PING_STAT_SHIFT     ( 0 )
#define TX_PING_PONG_STAT_BF_TX_PONG_STAT_SHIFT     ( 1 )
#define TX_PING_PONG_STAT_BF_TX_PING_STAT_MASK      ( BF_TX_PING_PONG_STAT_MASK << TX_PING_PONG_STAT_BF_TX_PING_STAT_SHIFT )
#define TX_PING_PONG_STAT_BF_TX_PONG_STAT_MASK      ( BF_TX_PING_PONG_STAT_MASK << TX_PING_PONG_STAT_BF_TX_PONG_STAT_SHIFT )

/* FW_REG_RX_PING_PONG_STAT */
#define BF_RX_PING_PONG_STAT_MASK                   ( 0x1 )
#define RX_PING_PONG_STAT_BF_RX_PING_STAT_SHIFT     ( 0 )
#define RX_PING_PONG_STAT_BF_RX_PONG_STAT_SHIFT     ( 1 )
#define RX_PING_PONG_STAT_BF_RX_PING_STAT_MASK     ( BF_RX_PING_PONG_STAT_MASK << RX_PING_PONG_STAT_BF_RX_PING_STAT_SHIFT )
#define RX_PING_PONG_STAT_BF_RX_PONG_STAT_MASK     ( BF_RX_PING_PONG_STAT_MASK << RX_PING_PONG_STAT_BF_RX_PONG_STAT_SHIFT )

/* FW_REG_ERR_STAT */
#define BF_OVR_ERR_STAT_MASK                        ( 0x1 )
#define BF_UND_ERR_STAT_MASK                        ( 0x1 )
#define BF_FSYNC_ERR_STAT_MASK                      ( 0x1 )
#define ERR_STAT_BF_OVR_ERR_STAT_SHIFT              ( 0 )
#define ERR_STAT_BF_UND_ERR_STAT_SHIFT              ( 1 )
#define ERR_STAT_BF_FSYNC_ERR_STAT_SHIFT            ( 2 )
#define ERR_STAT_BF_BF_OVR_ERR_STAT_MASK            ( BF_OVR_ERR_STAT_MASK << ERR_STAT_BF_OVR_ERR_STAT_SHIFT )
#define ERR_STAT_BF_BF_UND_ERR_STAT_MASK            ( BF_UND_ERR_STAT_MASK << ERR_STAT_BF_UND_ERR_STAT_SHIFT )
#define ERR_STAT_BF_BF_FSYNC_ERR_STAT_MASK          ( BF_FSYNC_ERR_STAT_MASK << ERR_STAT_BF_FSYNC_ERR_STAT_SHIFT )

#define PING_PONG_SEL_PING ( 0 )
#define PING_PONG_SEL_PONG ( 1 )

#endif /* _ICSS_PRUI2S_FW_H_ */
