/*
 * AM261x_PRU0.cmd
 *
 * Example Linker command file for linking programs built with the TI-PRU-CGT
 * on AM261x PRU0 cores
 *
 * Copyright (C) 2022-23 Texas Instruments Incorporated - https://www.ti.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the
 *	  distribution.
 *
 *	* Neither the name of Texas Instruments Incorporated nor the names of
 *	  its contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
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

/* Specify the System Memory Map */
MEMORY
{
    PAGE 0:
      /* 12 KB PRU Instruction RAM */
      PRU_IMEM          : org = 0x00000000 len = 0x00003000

    PAGE 1:
      /* Data RAMs */
      /* 8 KB PRU Data RAM 0 */
      PRU0_DMEM_0       : org = 0x00000000 len = 0x00002000
      /* 8 KB PRU Data RAM 1 */
      PRU0_DMEM_1       : org = 0x00002000 len = 0x00002000

    PAGE 2:
      /* C28 needs to be programmed to point to SHAREDMEM, default is 0 */
      /* 32 KB Shared general purpose memory RAM with ECC, shared between PRU0 and PRU1 */
      PRU_SHAREDMEM     : org = 0x00010000 len = 0x00008000
}

/* Specify the sections allocation into memory */
SECTIONS {

    .text           >  PRU_IMEM,    PAGE 0
    .bss            >  PRU0_DMEM_0, PAGE 1
    .stack          >  PRU0_DMEM_0, PAGE 1
}
