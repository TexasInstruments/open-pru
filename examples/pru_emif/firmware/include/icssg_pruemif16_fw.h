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

#ifndef _ICSSG_PRUEMIF16_FW_H_
#define _ICSSG_PRUEMIF16_FW_H_

/* 
    Firmware DMEM registers 
*/

/* FW register base address */
#define ICSSG_PRUEMIF16_DMEM_FW_REG_BASE            ( 0x0 )

/* FW register sizes (in bytes) */
#define FW_REG_FW_STAT_SZ                           ( 1 )
#define ICSSG_PRUEMIF16_DMEM_FW_REG_SZ              ( FW_REG_FW_STAT_SZ )

/* FW register offsets from base (in bytes) */
#define FW_REG_FW_STAT_OFFSET                       ( 0x00 )

/* FW register addresses */
#define FW_REG_FW_STAT                              ( ICSSG_PRUEMIF16_DMEM_FW_REG_BASE + FW_REG_FW_STAT_OFFSET )

/*
    Firmware register bit fields 
*/

/* FW_STAT:PRU_INIT */
#define BF_PRU_INIT_MASK                            ( 0x1 )
#define PRU_INIT_SHIFT                              ( 0 )
#define PRU_INIT_MASK                               ( BF_PRU_INIT_MASK << PRU_INIT_SHIFT )
#define PRU_INIT_RESET_VAL                          ( 0x0 << PRU_INIT_SHIFT )

/* FW_STAT:SHARE_ERR */
#define BF_SHARE_ERR_MASK                           ( 0x1 )
#define SHARE_ERR_SHIFT                             ( 1 )
#define SHARE_ERR_MASK                              ( BF_SHARE_ERR_MASK << SHARE_ERR_SHIFT )
#define SHARE_ERR_RESET_VAL                         ( 0x0 << SHARE_ERR_SHIFT )


#endif /* _ICSSG_PRUEMIF16_FW_H_ */
