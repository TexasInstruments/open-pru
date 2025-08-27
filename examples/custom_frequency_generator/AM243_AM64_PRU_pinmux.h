/*  ============================================================================
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
 *
*/

/* 
    file:   AM243_AM64_PRU_pinmux.inc

   brief:  Device specific pinmux defines for direct PRU configuration
           SDK driver on PRU would consume too much memory
           This is a trade off between readablility and code size.
 
   author: Thomas Leyrer
 
    (C) Copyright 2025, Texas Instruments, Inc
*/

#ifndef AM243_AM64_PRU_pinmux_h
#define AM243_AM64_PRU_pinmux_h

// LOCK UNLOCK defines
#define   PAD_PARTITION0_L0   (0x000f1008)
#define   PAD_PARTITION0_L1   (0x000f100C)
#define   PAD_PARTITION1_L0   (0x000f5008)
#define   PAD_PARTITION1_L1   (0x000f500C)

#define   PAD_KEY0            (0x68EF3490) 
#define   PAD_KEY1            (0xD172BC5A) 

// Input/Output defines
#define   PRU_GPI             (0x00040001)
#define   PRU_GPO             (0x00010000)
#define   IEP_SYNC_OUT        (0x00010002) 
#define   IEP_LATCH_IN        (0x00040002) 

// ICSS_G0 PRU0
#define   PRG0_PRU0_GPO0            (0x000F4160)
#define   PRG0_PRU0_GPO1            (0x000F4164)
#define   PRG0_PRU0_GPO2            (0x000F4168)
#define   PRG0_PRU0_GPO3            (0x000F416C)
#define   PRG0_PRU0_GPO4            (0x000F4170)
#define   PRG0_PRU0_GPO5            (0x000F4174)
#define   PRG0_PRU0_GPO6            (0x000F4178)
#define   PRG0_PRU0_GPO7            (0x000F417C)
#define   PRG0_PRU0_GPO8            (0x000F4180)
#define   PRG0_PRU0_GPO9            (0x000F4184)
#define   PRG0_PRU0_GPO10           (0x000F4188)
#define   PRG0_PRU0_GPO11           (0x000F418C)
#define   PRG0_PRU0_GPO12           (0x000F4190)
#define   PRG0_PRU0_GPO13           (0x000F4194)
#define   PRG0_PRU0_GPO14           (0x000F4198)
#define   PRG0_PRU0_GPO15           (0x000F419C)
#define   PRG0_PRU0_GPO16           (0x000F41A0)
#define   PRG0_PRU0_GPO17           (0x000F41A4)
#define   PRG0_PRU0_GPO18           (0x000F41A8)
#define   PRG0_PRU0_GPO19           (0x000F41AC)

// ICSS_G0 PRU1
#define   PRG0_PRU1_GPO0            (0x000F41B0)
#define   PRG0_PRU1_GPO1            (0x000F41B4)
#define   PRG0_PRU1_GPO2            (0x000F41B8)
#define   PRG0_PRU1_GPO3            (0x000F41BC)
#define   PRG0_PRU1_GPO4            (0x000F41C0)
#define   PRG0_PRU1_GPO5            (0x000F41C4)
#define   PRG0_PRU1_GPO6            (0x000F41C8)
#define   PRG0_PRU1_GPO7            (0x000F41CC)
#define   PRG0_PRU1_GPO8            (0x000F41D0)
#define   PRG0_PRU1_GPO9            (0x000F41D4)
#define   PRG0_PRU1_GPO10           (0x000F41D8)
#define   PRG0_PRU1_GPO11           (0x000F41DC)
#define   PRG0_PRU1_GPO12           (0x000F41E0)
#define   PRG0_PRU1_GPO13           (0x000F41E4)
#define   PRG0_PRU1_GPO14           (0x000F41E8)
#define   PRG0_PRU1_GPO15           (0x000F41EC)
#define   PRG0_PRU1_GPO16           (0x000F41F0)
#define   PRG0_PRU1_GPO17           (0x000F41F4)
#define   PRG0_PRU1_GPO18           (0x000F41F8)
#define   PRG0_PRU1_GPO19           (0x000F41FC)

// ICSS_G1 PRU0
#define    PRG1_PRU0_GPO0            (0x000F40B8)
#define    PRG1_PRU0_GPO1            (0x000F40BC)
#define    PRG1_PRU0_GPO2            (0x000F40C0)
#define    PRG1_PRU0_GPO3            (0x000F40C4)
#define    PRG1_PRU0_GPO4            (0x000F40C8)
#define    PRG1_PRU0_GPO5            (0x000F40CC)
#define    PRG1_PRU0_GPO6            (0x000F40D0)
#define    PRG1_PRU0_GPO7            (0x000F40D4)
#define    PRG1_PRU0_GPO8            (0x000F40D8)
#define    PRG1_PRU0_GPO9            (0x000F40DC)
#define    PRG1_PRU0_GP10            (0x000F40E0)
#define    PRG1_PRU0_GPO11           (0x000F40E4)
#define    PRG1_PRU0_GPO12           (0x000F40E8)
#define    PRG1_PRU0_GPO13           (0x000F40EC)
#define    PRG1_PRU0_GPO14           (0x000F40F0)
#define    PRG1_PRU0_GPO15           (0x000F40F4)
#define    PRG1_PRU0_GPO16           (0x000F40F8)
#define    PRG1_PRU0_GPO17           (0x000F40FC)
#define    PRG1_PRU0_GPO18           (0x000F4100)
#define    PRG1_PRU0_GPO19           (0x000F4104)
    
// ICSS_G1 PRU1
#define    PRG1_PRU1_GPO0            (0x000F4108)
#define    PRG1_PRU1_GPO1            (0x000F410C)
#define    PRG1_PRU1_GPO2            (0x000F4110)
#define    PRG1_PRU1_GPO3            (0x000F4114)
#define    PRG1_PRU1_GPO4            (0x000F4118)
#define    PRG1_PRU1_GPO5            (0x000F411C)
#define    PRG1_PRU1_GPO6            (0x000F4120)
#define    PRG1_PRU1_GPO7            (0x000F4124)
#define    PRG1_PRU1_GPO8            (0x000F4128)
#define    PRG1_PRU1_GPO9            (0x000F412C)
#define    PRG1_PRU1_GPO10           (0x000F4130)
#define    PRG1_PRU1_GPO11           (0x000F4134)
#define    PRG1_PRU1_GPO12           (0x000F4138)
#define    PRG1_PRU1_GPO13           (0x000F413C)
#define    PRG1_PRU1_GPO14           (0x000F4140)
#define    PRG1_PRU1_GPO15           (0x000F4144)
#define    PRG1_PRU1_GPO16           (0x000F4148)
#define    PRG1_PRU1_GPO17           (0x000F414C)
#define    PRG1_PRU1_GPO18           (0x000F4150)
#define    PRG1_PRU1_GPO19           (0x000F4154)

#endif
