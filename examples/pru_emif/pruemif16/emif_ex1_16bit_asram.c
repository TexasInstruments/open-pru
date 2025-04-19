//#############################################################################
//
// FILE:   emif_ex1_16bit_asram.c
//
// TITLE:  EMIF1 ASYNC module accessing 16bit ASRAM.
//
//! \addtogroup driver_example_list
//! <h1> EMIF1 ASYNC module accessing 16bit ASRAM. </h1>
//!
//! This example configures EMIF1 in 16bit ASYNC mode and uses CS2 as chip
//! enable.
//!
//! \b External \b Connections \n
//!  - External ASRAM memory (CY7C1041CV33 -10ZSXA) daughter card
//!
//! \b Watch \b Variables \n
//! - \b testStatusGlobal - Equivalent to \b TEST_PASS if test finished
//!                         correctly, else the value is set to \b TEST_FAIL
//! - \b errCountGlobal - Error counter
//!
//
//#############################################################################
//
// $Release Date: $
// $Copyright:
// Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//#############################################################################

//
// Included Files
//
#include "device.h"
#include "driverlib.h"

//
// Defines
//
#define TEST_PASS 0xABCDABCD
#define TEST_FAIL 0xDEADDEAD
#define ASRAM_CS2_START_ADDR 0x100000
#define ASRAM_CS2_SIZE 0x8000

//
// Globals
//
uint16_t errCountGlobal = 0;
uint32_t testStatusGlobal;
uint32_t i;

//
// Function Prototypes
//
extern void setupEMIF1PinmuxAsync16Bit(void);
uint16_t readWriteMem(uint32_t startAddr, uint32_t memSize);
uint16_t walkMemData(uint32_t startAddr, uint32_t memSize);
uint16_t walkMemAddr(uint32_t startAddr, uint32_t addrSize);
uint16_t accessMemData(uint32_t startAddr, uint32_t sizeToCheck);

// FL: PRU EMIF16
//
// customer EMIF settings
#define _UPDATE_EMIF_ASYNC
#define MAX_EXT_WAIT_CYCLES         ( 4 )
#define READ_STROBE_SETUP_CYCLES    ( 3 )
//#define READ_STROBE_CYCLES          ( 13 )
#define READ_STROBE_HOLD_CYCLES     ( 0 )
#define WRITE_STROBE_SETUP_CYCLES   ( 3 )
#define WRITE_STROBE_CYCLES         ( 13 )
#define WRITE_STROBE_HOLD_CYCLES    ( 1 )
#define TA_CYCLES                   ( 3 )

//#define MAX_EXT_WAIT_CYCLES         ( 8 )
#define READ_STROBE_CYCLES          ( 30 ) //( 26 )

// Test settings
#define _TEST_BASIC
#ifdef _TEST_BASIC

#define _TEST_BASIC_RD_WR_16B       // define for 16-bit tests
#define _TEST_BASIC_RD_WR_32B       // define for 32-bit tests
#define _TEST_BASIC_RD_WR_16B_32B   // define for 16/32-bit tests

//#define _DBG_DELAY                  // define for delay between tests
#define DELAY_LOOP_CNT              ( 10000 )
#endif

#define SHMEM_BUFFER_SZ             ( 256 ) /* 16-bit words */

// Data mask for unconnected data bus lines
#define DATA_MASK_16B   (6527UL)
#define DATA_MASK_32B   (DATA_MASK_16B<<16 | DATA_MASK_16B)

#define NUM_WR_16B      ( 8 )
#define NUM_WR_32B      ( 4 )
uint16_t gTestWr16Mem[2*NUM_WR_16B];
uint32_t gTestWr32Mem[2*NUM_WR_32B];

//
// Initialite data for 16-bit writes
//
void basicTestInitDataWr16Bit(void);

//
// Initialite data for 32-bit writes
//
void basicTestInitDataWr32Bit(void);

#if defined(_TEST_BASIC_RD_WR_16B)
#define NUM_RD_16B      ( 8 )
#define NUM_RD_WR_16B   ( 8 )
#define NUM_WR_RD_16B   ( 8 )
uint16_t gTestRd16Mem[2*NUM_RD_16B];
uint16_t gTestRdWr16Mem[2*NUM_RD_WR_16B];
uint16_t gTestWrRd16Mem[2*NUM_WR_RD_16B];

#define RD_16B_SOB_OFFSET           ( 0 )
#define RD_16B_EOB_OFFSET           ( 248 )
#define WR_16B_SOB_OFFSET           ( 8 )
#define WR_16B_EOB_OFFSET           ( 240 )
#define RD_WR_16B_SOB_OFFSET        ( 16 )
#define RD_WR_16B_EOB_OFFSET        ( 224 )
#define WR_RD_16B_SOB_OFFSET        ( 32 )
#define WR_RD_16B_EOB_OFFSET        ( 216 )

//
// Test 16-bit read
//
int16_t basicTestRd16Bit(
    uint32_t bufStartAddr,  // buffer start address
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufRdOffset,   // read offset (in 16-bit words)
    uint32_t numRd,         // number of 16-bit words to read
    uint16_t *pRdDst
);

//
// Test 16-bit write
//
int16_t basicTestWr16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufWrOffset, 
    uint32_t numWr,
    uint16_t *pWrSrc
);

//
// Test 16-bit read -> write
//
int16_t basicTestRdWr16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufRdOffset, 
    uint32_t bufWrOffset, 
    uint32_t numRdWr,
    uint16_t *pRdDst,
    uint16_t *pWrSrc
);

//
// Test 16-bit write -> read
//
int16_t basicTestWrRd16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufRdOffset, 
    uint32_t bufWrOffset, 
    uint32_t numRdWr,
    uint16_t *pRdDst,
    uint16_t *pWrSrc
);

#endif // _TEST_BASIC_RD_WR_16B

#if defined(_TEST_BASIC_RD_WR_32B)
#define NUM_RD_32B      ( 4 )
#define NUM_RD_WR_32B   ( 4 )
#define NUM_WR_RD_32B   ( 4 )

uint32_t gTestRd32Mem[2*NUM_RD_32B];
uint32_t gTestRdWr32Mem[2*NUM_RD_WR_32B];
uint32_t gTestWrRd32Mem[2*NUM_WR_RD_32B];

#define RD_32B_SOB_OFFSET           ( 40/2 )
#define RD_32B_EOB_OFFSET           ( 208/2 )
#define WR_32B_SOB_OFFSET           ( 48/2 )
#define WR_32B_EOB_OFFSET           ( 200/2 )
#define RD_WR_32B_SOB_OFFSET        ( 56/2 )
#define RD_WR_32B_EOB_OFFSET        ( 184/2 )
#define WR_RD_32B_SOB_OFFSET        ( 72/2 )
#define WR_RD_32B_EOB_OFFSET        ( 176/2 )

//
// Test 32-bit read
//
int16_t basicTestRd32Bit(
    uint32_t bufStartAddr,  // buffer start address
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t numRd,         // number of 32-bit words to read
    uint32_t *pRdDst        // pointer to read destination data
);

//
// Test 32-bit write
//
int16_t basicTestWr32Bit(
    uint32_t bufStartAddr,  // buffer start address
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numWr,         // number of 32-bit words to write
    uint32_t *pWrSrc        // pointer to source data
);

//
// Test 32-bit read -> write
//
int16_t basicTestRdWr32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numRdWr,       // number of 32-bit words to write
    uint32_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
);

//
// Test 32-bit write -> read
//
int16_t basicTestWrRd32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numRdWr,       // number of 32-bit words to write
    uint32_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
);

#endif // _TEST_BASIC_RD_WR_32B

#if defined(_TEST_BASIC_RD_WR_16B_32B)
#define NUM_RD_16B_WR_32B   ( 4 )   // Number of 16-bit reads and 32-bit writes
#define NUM_RD_32B_WR_16B   ( 4 )   // Number of 32-bit reads and 16-bit writes
uint16_t gTestRd16Wr32Mem[2*NUM_RD_16B_WR_32B];
uint32_t gTestRd32Wr16Mem[2*NUM_RD_32B_WR_16B];

#define NUM_WR_16B_RDBK_32B ( 8 )   // Number of 16-bit writes
#define NUM_WR_32B_RDBK_16B ( 8 )   // Number of 16-bit reads
uint32_t gTestWr16RdBk32Mem[2*NUM_WR_16B_RDBK_32B/2];
uint16_t gTestWr32RdBk16Mem[2*NUM_WR_32B_RDBK_16B];

#define RD_16B_WR_32B_RD_SOB_OFFSET ( 80 )
#define RD_16B_WR_32B_WR_SOB_OFFSET ( 88 )
#define RD_16B_WR_32B_RD_EOB_OFFSET ( 160 )
#define RD_16B_WR_32B_WR_EOB_OFFSET ( 168 )

#define RD_32B_WR_16B_RD_SOB_OFFSET ( 96 )
#define RD_32B_WR_16B_WR_SOB_OFFSET ( 104 )
#define RD_32B_WR_16B_RD_EOB_OFFSET ( 144 )
#define RD_32B_WR_16B_WR_EOB_OFFSET ( 152 )

#define WR_16B_RDBK_32B_SOB_OFFSET  ( 112 )
#define WR_16B_RDBK_32B_EOB_OFFSET  ( 136 )

#define WR_32B_RDBK_16B_SOB_OFFSET  ( 120 )
#define WR_32B_RDBK_16B_EOB_OFFSET  ( 128 )

//
// Test 16-bit read -> 32-bit write
//
int16_t basicTestRd16BitWr32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufRdOffset,   // read offset (in 16-bit words)
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numRdWr,       // number of 16-bit words to read / 32-bit words to write
    uint16_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
);

//
// Test 32-bit read -> 16-bit write
//
int16_t basicTestRd32BitWr16Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t bufWrOffset,   // write offset (in 16-bit words)
    uint32_t numRdWr,       // number of 32-bit words to read / 16-bit words to write
    uint32_t *pRdDst,       // pointer to read destination data
    uint16_t *pWrSrc        // pointer to write source data
);

//
// Test 16-bit write -> 32-bit read back
//
int16_t basicTestWr16BitRdBk32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufOffset,     // write/read offset (in 16-bit words)
    uint32_t numWr,         // number of 16-bit words to write, 1/2 number of 32-bit words to read back
    uint32_t *pRdDst,       // pointer to read destination data
    uint16_t *pWrSrc        // pointer to write source data
);

#endif // _TEST_BASIC_RD_WR_32B


//
// Test 32-bit write -> 16-bit read back
//
int16_t basicTestWr32BitRdBk16Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufOffset,     // write/read offset (in 16-bit words)
    uint32_t numRd,         // 1/2 number of 32-bit words to write, number of 16-bit words to read back
    uint16_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
);


//
// Main
//
void main(void)
{
    uint16_t errCountLocal;
    EMIF_AsyncTimingParams tparam;
    testStatusGlobal = TEST_FAIL;
#ifdef _TEST_BASIC
    int32_t status;
#ifdef _DBG_DELAY
    uint32_t delayLoop;
#endif
#endif    

    //
    // Initialize device clock and peripherals.
    //
    Device_init();

    //
    // Disable all the interrupts.
    //
    DINT;

    //
    // Setup GPIO by disabling pin locks and enabling pullups.
    //
    Device_initGPIO();

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    //
    Interrupt_initVectorTable();

    //
    // Configure to run EMIF1 on full Rate. (EMIF1CLK = CPU1SYSCLK)
    //
    SysCtl_setEMIF1ClockDivider(SYSCTL_EMIF1CLK_DIV_1);

    //
    // Grab EMIF1 For CPU1.
    //
    EMIF_selectMaster(EMIF1CONFIG_BASE, EMIF_MASTER_CPU1_G);

    //
    // Disable Access Protection. (CPU_FETCH/CPU_WR/DMA_WR)
    //
    EMIF_setAccessProtection(EMIF1CONFIG_BASE, 0x0);

    //
    // Commit the configuration related to protection. Till this bit remains
    // set, contents of EMIF1ACCPROT0 register can't be changed.
    //
    EMIF_commitAccessConfig(EMIF1CONFIG_BASE);

    //
    // Lock the configuration so that EMIF1COMMIT register can't be changed
    // any more.
    //
    EMIF_lockAccessConfig(EMIF1CONFIG_BASE);

    //
    // Configure GPIO pins for EMIF1.
    //
    setupEMIF1PinmuxAsync16Bit();

    //
    // Configures Normal Asynchronous Mode of Operation.
    //
    EMIF_setAsyncMode(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET,
                      EMIF_ASYNC_NORMAL_MODE);

#ifndef _UPDATE_EMIF_ASYNC
    //
    // Disables Extended Wait Mode.
    //
    EMIF_disableAsyncExtendedWait(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET);
#else
    //
    // Enables Extended Wait Mode.
    //
    EMIF_enableAsyncExtendedWait(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET);
#endif

#ifdef _UPDATE_EMIF_ASYNC
    //
    // Set the Maximum Wait Cycles
    //
    EMIF_setAsyncMaximumWaitCycles(EMIF1_BASE, MAX_EXT_WAIT_CYCLES);

    //
    // Set the wait polarity
    //
    EMIF_setAsyncWaitPolarity(EMIF1_BASE, EMIF_ASYNC_WAIT_POLARITY_LOW);
#endif


    //
    // Configure EMIF1 Data Bus Width.
    //
    EMIF_setAsyncDataBusWidth(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET,
                              EMIF_ASYNC_DATA_WIDTH_16);

    //
    // Configure the access timing for CS2 space.
    //
#ifndef _UPDATE_EMIF_ASYNC
    tparam.rSetup = 0;
    tparam.rStrobe = 3;
    tparam.rHold = 0;
    tparam.turnArnd = 0;
    tparam.wSetup = 0;
    tparam.wStrobe = 1;
    tparam.wHold = 0;
#else
    tparam.rSetup = READ_STROBE_SETUP_CYCLES;
    tparam.rStrobe = READ_STROBE_CYCLES;
    tparam.rHold = READ_STROBE_HOLD_CYCLES;
    tparam.turnArnd = TA_CYCLES;
    tparam.wSetup = WRITE_STROBE_SETUP_CYCLES;
    tparam.wStrobe = WRITE_STROBE_CYCLES;
    tparam.wHold = WRITE_STROBE_HOLD_CYCLES;
#endif
    EMIF_setAsyncTimingParams(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET, &tparam);

#ifndef _TEST_BASIC
    //
    // Checks basic RD/WR access to CS2 space.
    //
    errCountLocal = readWriteMem(ASRAM_CS2_START_ADDR, ASRAM_CS2_SIZE);
    errCountGlobal = errCountGlobal + errCountLocal;

    //
    // Address walk checks. (Tested for Memory with address width of 16bit)
    //
    errCountLocal = walkMemAddr(ASRAM_CS2_START_ADDR, 16);
    errCountGlobal = errCountGlobal + errCountLocal;

    //
    // Data walk checks.
    //
    errCountLocal = walkMemData(ASRAM_CS2_START_ADDR, ASRAM_CS2_SIZE);
    errCountGlobal = errCountGlobal + errCountLocal;

    //
    // Data size checks.
    //
    errCountLocal = accessMemData(ASRAM_CS2_START_ADDR, 4);
    errCountGlobal = errCountGlobal + errCountLocal;

    if(errCountGlobal == 0x0)
    {
        testStatusGlobal = TEST_PASS;
    }

#else // _TEST_BASIC

    /* Initialize write data */
    basicTestInitDataWr16Bit();
    basicTestInitDataWr32Bit();

#if defined(_TEST_BASIC_RD_WR_16B)
    /* Read 16-bit */
    /* read toward start of buffer */
    status = basicTestRd16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_16B_SOB_OFFSET, 
        NUM_RD_16B, 
        &gTestRd16Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* read toward end of buffer */
    status = basicTestRd16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_16B_EOB_OFFSET, 
        NUM_RD_16B, 
        &gTestRd16Mem[NUM_RD_16B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif    

    /* Write 16-bit  */
    /* write toward start of buffer */
    status = basicTestWr16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_16B_SOB_OFFSET, 
        NUM_WR_16B, 
        &gTestWr16Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* write toward end of buffer */
    status = basicTestWr16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_16B_EOB_OFFSET, 
        NUM_WR_16B, 
        &gTestWr16Mem[NUM_WR_16B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
    
    /* Read -> Write 16-bit  */
    /* read/write toward start of buffer */
    status = basicTestRdWr16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_WR_16B_SOB_OFFSET, 
        RD_WR_16B_SOB_OFFSET+NUM_RD_WR_16B, 
        NUM_RD_WR_16B, 
        &gTestRdWr16Mem[0], 
        &gTestWr16Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* read/write toward end of buffer */
    status = basicTestRdWr16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_WR_16B_EOB_OFFSET+NUM_RD_WR_16B,
        RD_WR_16B_EOB_OFFSET, 
        NUM_RD_WR_16B, 
        &gTestRdWr16Mem[NUM_RD_WR_16B], 
        &gTestWr16Mem[NUM_RD_WR_16B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
    
    /* Write -> Read Back 16-bit  */
    /* write/read toward start of buffer */
    status = basicTestWrRd16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_RD_16B_SOB_OFFSET, 
        WR_RD_16B_SOB_OFFSET, 
        NUM_WR_RD_16B, 
        &gTestWrRd16Mem[0], 
        &gTestWr16Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* write/read toward end of buffer */
    status = basicTestWrRd16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_RD_16B_EOB_OFFSET, 
        WR_RD_16B_EOB_OFFSET, 
        NUM_WR_RD_16B, 
        &gTestWrRd16Mem[NUM_WR_RD_16B], 
        &gTestWr16Mem[NUM_WR_RD_16B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif

#endif // _TEST_BASIC_RD_WR_16B

#if defined(_TEST_BASIC_RD_WR_32B)
    /* Read 32-bit */
    /* read toward start of buffer */
    status = basicTestRd32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ/2, 
        RD_32B_SOB_OFFSET, 
        NUM_RD_32B, 
        &gTestRd32Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* read toward end of buffer */
    status = basicTestRd32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ/2,
        RD_32B_EOB_OFFSET,
        NUM_RD_32B, 
        &gTestRd32Mem[NUM_RD_32B]);
    if (status != 0)
    {
        while(1);
    }
    
#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif

    /* Write 32-bit */
    /* write toward start of buffer */
    status = basicTestWr32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ/2, 
        WR_32B_SOB_OFFSET,
        NUM_WR_32B, 
        &gTestWr32Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* write  toward end of buffer */
    status = basicTestWr32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ/2,
        WR_32B_EOB_OFFSET,
        NUM_WR_32B, 
        &gTestWr32Mem[NUM_WR_32B]);
    if (status != 0)
    {
        while(1);
    }
    
#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
  
    /* Read -> Write 32-bit  */
    /* read/write toward start of buffer */
    status = basicTestRdWr32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_WR_32B_SOB_OFFSET, 
        RD_WR_32B_SOB_OFFSET+NUM_RD_WR_32B, 
        NUM_RD_WR_32B, 
        &gTestRdWr32Mem[0], 
        &gTestWr32Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* read/write toward end of buffer */
    status = basicTestRdWr32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_WR_32B_EOB_OFFSET+NUM_RD_WR_32B,
        RD_WR_32B_EOB_OFFSET,
        NUM_RD_WR_32B, 
        &gTestRdWr32Mem[NUM_RD_WR_32B], 
        &gTestWr32Mem[NUM_RD_WR_32B]);
    if (status != 0)
    {
        while(1);
    }
    
#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
    
    /* Write -> Read Back 32-bit  */
    /* write/read toward start of buffer */
    status = basicTestWrRd32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_RD_32B_SOB_OFFSET, 
        WR_RD_32B_SOB_OFFSET, 
        NUM_WR_RD_32B, 
        &gTestWrRd32Mem[0], 
        &gTestWr32Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* write/read toward end of buffer */
    status = basicTestWrRd32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_RD_32B_EOB_OFFSET, 
        WR_RD_32B_EOB_OFFSET, 
        NUM_WR_RD_32B, 
        &gTestWrRd32Mem[NUM_WR_RD_32B], 
        &gTestWr32Mem[NUM_WR_RD_32B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif

#endif // _TEST_BASIC_RD_WR_32B
    
#if defined(_TEST_BASIC_RD_WR_16B_32B)
    /* Read 16-bit -> Write 32-bit  */
    /* read/write toward start of buffer */
    status = basicTestRd16BitWr32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_16B_WR_32B_RD_SOB_OFFSET, 
        RD_16B_WR_32B_WR_SOB_OFFSET/2,
        NUM_RD_16B_WR_32B, 
        &gTestRd16Wr32Mem[0],
        &gTestWr32Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* read/write toward end of buffer */
    status = basicTestRd16BitWr32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_16B_WR_32B_RD_EOB_OFFSET,
        RD_16B_WR_32B_WR_EOB_OFFSET/2,
        NUM_RD_16B_WR_32B, 
        &gTestRd16Wr32Mem[NUM_RD_16B_WR_32B],
        &gTestWr32Mem[NUM_RD_16B_WR_32B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
    
    /* Read 32-bit -> Write 16-bit  */
    /* read/write toward start of buffer */
    status = basicTestRd32BitWr16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_32B_WR_16B_RD_SOB_OFFSET/2,
        RD_32B_WR_16B_WR_SOB_OFFSET,
        NUM_RD_32B_WR_16B, 
        &gTestRd32Wr16Mem[0],
        &gTestWr16Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* read/write toward end of buffer */
    status = basicTestRd32BitWr16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        RD_32B_WR_16B_RD_EOB_OFFSET/2,
        RD_32B_WR_16B_WR_EOB_OFFSET,
        NUM_RD_32B_WR_16B, 
        &gTestRd32Wr16Mem[NUM_RD_32B_WR_16B],
        &gTestWr16Mem[NUM_RD_32B_WR_16B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
    
    /* Write 16-bit -> Read back 32-bit  */
    /* write/read toward start of buffer */
    status = basicTestWr16BitRdBk32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_16B_RDBK_32B_SOB_OFFSET, 
        NUM_WR_16B_RDBK_32B, 
        &gTestWr16RdBk32Mem[0/2],
        &gTestWr16Mem[0]);
    if (status != 0)
    {
        while(1);
    }
    /* write/read toward end of buffer */
    status = basicTestWr16BitRdBk32Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_16B_RDBK_32B_EOB_OFFSET, 
        NUM_WR_16B_RDBK_32B, 
        &gTestWr16RdBk32Mem[NUM_WR_16B_RDBK_32B/2],
        &gTestWr16Mem[NUM_WR_16B_RDBK_32B]);
    if (status != 0)
    {
        while(1);
    }

#ifdef _DBG_DELAY
    // dummy delay
    for (delayLoop = 0; delayLoop < DELAY_LOOP_CNT; delayLoop++)
    {
        ;
    }
#endif
    
    /* Write 32-bit -> Read back 16-bit  */
    /* write/read toward start of buffer */
    status = basicTestWr32BitRdBk16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_32B_RDBK_16B_SOB_OFFSET, 
        NUM_WR_32B_RDBK_16B, 
        &gTestWr32RdBk16Mem[0],
        &gTestWr32Mem[0/2]);
    if (status != 0)
    {
        while(1);
    }
    /* write/read toward end of buffer */
    status = basicTestWr32BitRdBk16Bit(ASRAM_CS2_START_ADDR, 
        SHMEM_BUFFER_SZ, 
        WR_32B_RDBK_16B_EOB_OFFSET, 
        NUM_WR_32B_RDBK_16B, 
        &gTestWr32RdBk16Mem[NUM_WR_32B_RDBK_16B],
        &gTestWr32Mem[NUM_WR_32B_RDBK_16B/2]);
    if (status != 0)
    {
        while(1);
    }

#endif // _TEST_BASIC_RD_WR_16B_32B

#endif // _TEST_BASIC

    while(1);
}

//
// Read Write Memory - This function performs simple read/write word accesses
// to memory.
//
uint16_t readWriteMem(uint32_t startAddr, uint32_t memSize)
{
    uint32_t memReadData;
    uint32_t memWriteData;
    uint32_t *memPtr;
    uint32_t i;

    memPtr = (uint32_t *)startAddr;

    //
    // Write data to memory.
    //
    memWriteData = 0x01234567;
    for(i = 0; i < memSize; i++)
    {
        *memPtr++ = memWriteData;
        memWriteData += 0x11111111;
    }

    //
    // Verify data written to memory.
    //
    memWriteData = 0x01234567;
    memPtr = (uint32_t *)startAddr;
    for(i = 0; i < memSize; i++)
    {
        memReadData = *memPtr;
        if(memReadData != memWriteData)
        {
            //return(1);
        }
        memPtr++;
        memWriteData += 0x11111111;
    }
    return(0);
}

//
// Walk Memory Data - This function performs a walking 0 & 1 on data lines
// for SRAM RD & WR.
//
uint16_t walkMemData(uint32_t startAddr, uint32_t memSize)
{
    uint32_t sramReadData;
    uint32_t sramWriteData;
    uint32_t i;
    uint32_t k;
    uint32_t m;
    uint32_t *memPtr;
    uint32_t *memPtrIter;

    memPtr = (uint32_t *)startAddr;

    for(i = 0; i < memSize; i = i + 64)
    {
        for(m = 0; m < 2; m++)
        {
            //
            // Write loop.
            //
            memPtrIter = memPtr;
            sramWriteData = 0x01;
            for(k = 0; k < 32; k++)
            {
                if(m == 0)
                {
                    *memPtrIter++ = sramWriteData;
                }
                else
                {
                    *memPtrIter++ = ~sramWriteData;
                }
                sramWriteData = sramWriteData << 1;
            }

            //
            // Read loop.
            //
            memPtrIter = memPtr;
            sramWriteData = 0x01;
            for(k = 0; k < 32; k++)
            {
                sramReadData = *memPtrIter;
                if(m == 1)
                {
                    sramReadData = ~sramReadData;
                }
                if(sramReadData != sramWriteData)
                {
                    return(1);
                }
                memPtrIter++;
                sramWriteData = sramWriteData << 1;
            }
        }
        memPtr = memPtrIter;
    }
    return(0);
}

//
// Walk Memory Addresses - This function performs a toggle on each address bit.
//
uint16_t walkMemAddr(uint32_t startAddr, uint32_t addrSize)
{
    uint32_t sramReadData;
    uint32_t sramWriteData;
    uint32_t k;
    uint32_t xshift;
    uint32_t *memPtr;
    uint32_t *memPtrIter;

    memPtr = (uint32_t *)startAddr;

    //
    // Write loop.
    //
    xshift = 0x00000001;
    sramWriteData = 0x00;
    for(k = 0; k < addrSize; k++)
    {
        memPtrIter = (uint32_t *)(startAddr + xshift);
        *memPtrIter = sramWriteData++;
        xshift = xshift << 1;
    }

    //
    // Read loop.
    //
    memPtrIter = memPtr;
    xshift = 0x00000001;
    sramWriteData = 0x00;
    for(k = 0; k < addrSize; k++)
    {
        memPtrIter = (uint32_t *)(startAddr + xshift);
        sramReadData = *memPtrIter;
        if(sramReadData != sramWriteData)
        {
            return(1);
        }
        xshift = xshift << 1;
        sramWriteData++;
    }
    return(0);
}

//
// Access Memory Data - This function performs different data type
// (HALFWORD/WORD) access.
//
uint16_t accessMemData(uint32_t startAddr, uint32_t sizeToCheck)
{
    uint16_t memRdShort;
    uint32_t memRdLong;
    uint16_t memWrShort;
    uint32_t memWrLong;
    uint32_t i;
    uint16_t *memPtrShort;
    uint32_t *memPtrLong;

    //
    // Write short data.
    //
    memPtrShort = (uint16_t *)startAddr;
    memWrShort = 0x0605;

    for(i = 0; i < 2; i++)
    {
        *memPtrShort++ = memWrShort;
        memWrShort += 0x0202;
    }

#if 0 // FL:add, testing
    //
    // Write short data.
    //
    memPtrShort = (uint16_t *)startAddr;
    memWrShort = 0x0605;

    for(i = 0; i < 32768 ; i++)
    {
        *memPtrShort++ = memWrShort;
        memWrShort += 0x0202;
    }
#endif

    //
    // Write long data.
    //
    memPtrLong = (uint32_t *)memPtrShort;
    memWrLong = 0x0C0B0A09;
    for(i = 0; i < 2; i++)
    {
        *memPtrLong++ = memWrLong;
        memWrLong += 0x04040404;
    }

    //
    // Read short data.
    //
    memPtrShort = (uint16_t *)startAddr;
    memWrShort = 0x0605;
    for(i = 0; i < 6; i++)
    {
        memRdShort = *memPtrShort;
        if(memRdShort != memWrShort)
        {
            //return(1);
        }
        memPtrShort++;
        memWrShort += 0x0202;
    }

    //
    // Read long data.
    //
    memPtrLong = (uint32_t *)startAddr;
    memWrLong = 0x08070605;
    for(i = 0; i < 3; i++)
    {
        memRdLong = *memPtrLong;
        if(memRdLong != memWrLong)
        {
            return(1);
        }
        memPtrLong++;
        memWrLong += 0x04040404;
    }
    return(0);
}

#ifdef _TEST_BASIC

//
// Initialite data for 16-bit writes
//
void basicTestInitDataWr16Bit(void)
{
    uint16_t memWrShort;
    uint32_t i;

    memWrShort = 0x0101;
    for (i = 0; i < NUM_WR_16B; i++)
    {
        gTestWr16Mem[i] = memWrShort & DATA_MASK_16B;
        memWrShort += 0x0101;
    }
    memWrShort = 0xFAFA;
    for (i = NUM_WR_16B; i < 2*NUM_WR_16B; i++)
    {
        gTestWr16Mem[i] = memWrShort & DATA_MASK_16B;
        memWrShort += 0x101;
    }
}

//
// Initialize data for 32-bit writes
//
void basicTestInitDataWr32Bit(void)
{
    uint32_t memWrLong;
    uint32_t i;

    memWrLong = 0xFAFA0101;
    for (i = 0; i < NUM_WR_32B; i++)
    {
        gTestWr32Mem[i] = memWrLong & DATA_MASK_32B;
        memWrLong += 0x01010101;
    }
    memWrLong = 0x0101FAFA;
    for (i = NUM_WR_32B; i < 2*NUM_WR_32B; i++)
    {
        gTestWr32Mem[i] = memWrLong & DATA_MASK_32B;
        memWrLong -= 0x01010101;
    }
}

#if defined(_TEST_BASIC_RD_WR_16B)
//
// Test 16-bit read
//
int16_t basicTestRd16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufRdOffset, 
    uint32_t numRd,
    uint16_t *pRdDst
)
{
    uint16_t *memPtrShort;
    uint32_t i;

    if ((bufRdOffset + numRd) > bufLength)
    {
        return -1;
    }

    // Read 16-bit words 
    memPtrShort = (uint16_t *)(bufStartAddr + bufRdOffset);
    for (i = 0; i < numRd; i++)
    {
        *pRdDst = *memPtrShort++;
        *pRdDst &= DATA_MASK_16B; // apply data mask for unconnected data lines
        pRdDst++;
    }
    
    return 0;
}

//
// Test 16-bit write
//
int16_t basicTestWr16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufWrOffset, 
    uint32_t numWr,
    uint16_t *pWrSrc
)
{
    uint16_t *memPtrShort;
    uint32_t i;

    if ((bufWrOffset + numWr) > bufLength)
    {
        return -1;
    }

    // Write 16-bit words at start of buffer
    memPtrShort = (uint16_t *)(bufStartAddr + bufWrOffset);
    for (i = 0; i < numWr; i++)
    {
        *memPtrShort++ = *pWrSrc & DATA_MASK_16B; // apply data mask for unconnected data lines
        pWrSrc++;
    }
    
    return(0);
}

//
// Test 16-bit read -> write
//
int16_t basicTestRdWr16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufRdOffset, 
    uint32_t bufWrOffset, 
    uint32_t numRdWr,
    uint16_t *pRdDst,
    uint16_t *pWrSrc
)
{
    uint16_t *memPtrRdShort;
    uint16_t *memPtrWrShort;
    uint32_t i;

    if (((bufRdOffset + numRdWr) > bufLength) ||
        ((bufWrOffset + numRdWr) > bufLength))
    {
        return -1;
    }

    memPtrRdShort = (uint16_t *)(bufStartAddr + bufRdOffset);
    memPtrWrShort = (uint16_t *)(bufStartAddr + bufWrOffset);
    for (i = 0; i < numRdWr; i++)
    {
        // read data
        *pRdDst = *memPtrRdShort++;
        *pRdDst &= DATA_MASK_16B; // apply data mask for unconnected data lines
        pRdDst++;

        // write data
        *memPtrWrShort++ = *pWrSrc++ & DATA_MASK_16B; // apply data mask for unconnected data lines
    }
    
    return 0;
}

//
// Test 16-bit write -> read
//
int16_t basicTestWrRd16Bit(
    uint32_t bufStartAddr, 
    uint32_t bufLength, 
    uint32_t bufRdOffset, 
    uint32_t bufWrOffset, 
    uint32_t numRdWr,
    uint16_t *pRdDst,
    uint16_t *pWrSrc
)
{
    uint16_t *memPtrRdShort;
    uint16_t *memPtrWrShort;
    uint32_t i;

    if (((bufRdOffset + numRdWr) > bufLength) ||
        ((bufWrOffset + numRdWr) > bufLength))
    {
        return -1;
    }

    memPtrRdShort = (uint16_t *)(bufStartAddr + bufRdOffset);
    memPtrWrShort = (uint16_t *)(bufStartAddr + bufWrOffset);
    for (i = 0; i < numRdWr; i++)
    {
        // write data
        *memPtrWrShort++ = *pWrSrc++;

        // read data
        *pRdDst = *memPtrRdShort++;
        *pRdDst &= DATA_MASK_16B; // apply data mask for unconnected data lines
        pRdDst++;
    }
    
    return 0;
}

#endif // _TEST_BASIC_RD_WR_16B

#if defined(_TEST_BASIC_RD_WR_32B)
//
// Test 32-bit read
//
int16_t basicTestRd32Bit(
    uint32_t bufStartAddr,  // buffer start address
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t numRd,         // number of 32-bit words to read
    uint32_t *pRdDst
)
{
    uint32_t *memPtrLong;
    uint32_t i;

    if ((bufRdOffset + numRd) > bufLength)
    {
        return -1;
    }

    // Read 32-bit words 
    memPtrLong = (uint32_t *)bufStartAddr;
    memPtrLong += bufRdOffset;
    for (i = 0; i < numRd; i++)
    {
        *pRdDst = *memPtrLong++;
        *pRdDst &= DATA_MASK_32B; // apply data mask for unconnected data lines
        pRdDst++;
    }
    
    return 0;
}

//
// Test 32-bit write
//
int16_t basicTestWr32Bit(
    uint32_t bufStartAddr,  // buffer start address
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numWr,         // number of 32-bit words to write
    uint32_t *pWrSrc
)
{
    uint32_t *memPtrLong;
    uint32_t i;

    if ((bufWrOffset + numWr) > bufLength)
    {
        return -1;
    }

    // Read 32-bit words 
    memPtrLong = (uint32_t *)bufStartAddr;
    memPtrLong += bufWrOffset;
    for (i = 0; i < numWr; i++)
    {
        *memPtrLong++ = *pWrSrc & DATA_MASK_32B; // apply data mask for unconnected data lines
        pWrSrc++;
    }
    
    return 0;
}

//
// Test 32-bit read -> write
//
int16_t basicTestRdWr32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numRdWr,       // number of 32-bit words to write
    uint32_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
)
{
    uint32_t *memPtrRdLong;
    uint32_t *memPtrWrLong;
    uint32_t i;

    if (((bufRdOffset + numRdWr) > bufLength) ||
        ((bufWrOffset + numRdWr) > bufLength))
    {
        return -1;
    }

    memPtrRdLong = (uint32_t *)bufStartAddr;
    memPtrRdLong += bufRdOffset;
    memPtrWrLong = (uint32_t *)bufStartAddr;
    memPtrWrLong += bufWrOffset;
    for (i = 0; i < numRdWr; i++)
    {
        // read data
        *pRdDst = *memPtrRdLong++;
        *pRdDst &= DATA_MASK_32B; // apply data mask for unconnected data lines
        pRdDst++;

        // write data
        *memPtrWrLong++ = *pWrSrc++ & DATA_MASK_32B; // apply data mask for unconnected data lines
    }
    
    return 0;
}

//
// Test 32-bit write -> read
//
int16_t basicTestWrRd32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 32-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numRdWr,       // number of 32-bit words to write
    uint32_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
)
{
    uint32_t *memPtrRdLong;
    uint32_t *memPtrWrLong;
    uint32_t i;

    if (((bufRdOffset + numRdWr) > bufLength) ||
        ((bufWrOffset + numRdWr) > bufLength))
    {
        return -1;
    }

    memPtrRdLong = (uint32_t *)bufStartAddr;
    memPtrRdLong += bufRdOffset;
    memPtrWrLong = (uint32_t *)bufStartAddr;
    memPtrWrLong += bufWrOffset;
    for (i = 0; i < numRdWr; i++)
    {
        // write data
        *memPtrWrLong++ = *pWrSrc++ & DATA_MASK_32B; // apply data mask for unconnected data lines

        // read data
        *pRdDst = *memPtrRdLong++;
        *pRdDst &= DATA_MASK_32B; // apply data mask for unconnected data lines
        pRdDst++;
    }
    
    return 0;
}

#endif // _TEST_BASIC_RD_WR_32B

#if defined(_TEST_BASIC_RD_WR_16B_32B)
//
// Test 16-bit read -> 32-bit write
//
int16_t basicTestRd16BitWr32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufRdOffset,   // read offset (in 16-bit words)
    uint32_t bufWrOffset,   // write offset (in 32-bit words)
    uint32_t numRdWr,       // number of 16-bit words to read / 32-bit words to write
    uint16_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
)
{
    uint16_t *memPtrRdShort;
    uint32_t *memPtrWrLong;
    uint32_t i;

    if ((bufRdOffset + numRdWr) > bufLength)
    {
        return -1;
    }
    if ((bufWrOffset + numRdWr) > bufLength/2)
    {
        return -1;
    }

    memPtrRdShort = (uint16_t *)bufStartAddr;
    memPtrRdShort += bufRdOffset;
    memPtrWrLong = (uint32_t *)bufStartAddr;
    memPtrWrLong += bufWrOffset;
    
    for (i = 0; i < numRdWr; i++)
    {
        // read data
        *pRdDst = *memPtrRdShort++;
        *pRdDst &= DATA_MASK_16B; // apply data mask for unconnected data lines
        pRdDst++;

        // write data
        *memPtrWrLong++ = *pWrSrc++ & DATA_MASK_32B; // apply data mask for unconnected data lines
    }
    
    return 0;
}

//
// Test 32-bit read -> 16-bit write
//
int16_t basicTestRd32BitWr16Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufRdOffset,   // read offset (in 32-bit words)
    uint32_t bufWrOffset,   // write offset (in 16-bit words)
    uint32_t numRdWr,       // number of 32-bit words to read / 16-bit words to write
    uint32_t *pRdDst,       // pointer to read destination data
    uint16_t *pWrSrc        // pointer to write source data
)
{
    uint32_t *memPtrRdLong;
    uint16_t *memPtrWrShort;
    uint32_t i;

    if ((bufRdOffset + numRdWr) > bufLength/2)
    {
        return -1;
    }
    if ((bufWrOffset + numRdWr) > bufLength)
    {
        return -1;
    }

    memPtrRdLong = (uint32_t *)bufStartAddr;
    memPtrRdLong += bufRdOffset;
    memPtrWrShort = (uint16_t *)bufStartAddr;
    memPtrWrShort += bufWrOffset;
    
    for (i = 0; i < numRdWr; i++)
    {
        // read data
        *pRdDst = *memPtrRdLong++;
        *pRdDst &= DATA_MASK_32B; // apply data mask for unconnected data lines
        pRdDst++;

        // write data
        *memPtrWrShort++ = *pWrSrc++ & DATA_MASK_16B; // apply data mask for unconnected data lines
    }
    
    return 0;
}

//
// Test 16-bit write -> 32-bit read back
//
int16_t basicTestWr16BitRdBk32Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufOffset,     // write/read offset (in 16-bit words)
    uint32_t numWr,         // number of 16-bit words to write, 1/2 number of 32-bit words to read back
    uint32_t *pRdDst,       // pointer to read destination data
    uint16_t *pWrSrc        // pointer to write source data
)
{
    uint32_t *memPtrRdLong;
    uint16_t *memPtrWrShort;
    uint32_t i;

    if ((bufOffset + numWr) > bufLength)
    {
        return -1;
    }

    memPtrRdLong = (uint32_t *)bufStartAddr;
    memPtrRdLong += bufOffset/2;
    memPtrWrShort = (uint16_t *)bufStartAddr;
    memPtrWrShort += bufOffset;
    
    for (i = 0; i < numWr/2; i++)
    {
        // write data
        *memPtrWrShort++ = *pWrSrc++ & DATA_MASK_16B; // apply data mask for unconnected data lines
        // write data
        *memPtrWrShort++ = *pWrSrc++ & DATA_MASK_16B; // apply data mask for unconnected data lines

        // read data
        *pRdDst = *memPtrRdLong++;
        *pRdDst &= DATA_MASK_32B; // apply data mask for unconnected data lines
        pRdDst++;
    }
    
    return 0;
}

//
// Test 32-bit write -> 16-bit read back
//
int16_t basicTestWr32BitRdBk16Bit(
    uint32_t bufStartAddr,  // buffer start address 
    uint32_t bufLength,     // buffer length in 16-bit words
    uint32_t bufOffset,     // write/read offset (in 16-bit words)
    uint32_t numRd,         // 1/2 number of 32-bit words to write, number of 16-bit words to read back
    uint16_t *pRdDst,       // pointer to read destination data
    uint32_t *pWrSrc        // pointer to write source data
)
{
    uint16_t *memPtrRdShort;
    uint32_t *memPtrWrLong;
    uint32_t i;

    if ((bufOffset + numRd) > bufLength)
    {
        return -1;
    }

    memPtrRdShort = (uint16_t *)bufStartAddr;
    memPtrRdShort += bufOffset;
    memPtrWrLong = (uint32_t *)bufStartAddr;
    memPtrWrLong += bufOffset/2;
    
    for (i = 0; i < numRd/2; i++)
    {
        // write data
        *memPtrWrLong++ = *pWrSrc++ & DATA_MASK_32B; // apply data mask for unconnected data lines

        // read data
        *pRdDst = *memPtrRdShort++;
        *pRdDst &= DATA_MASK_16B; // apply data mask for unconnected data lines
        pRdDst++;
        // read data
        *pRdDst = *memPtrRdShort++;
        *pRdDst &= DATA_MASK_16B; // apply data mask for unconnected data lines
        pRdDst++;
    }
    
    return 0;
}

#endif // _TEST_BASIC_RD_WR_16B_32B

#endif // _TEST_BASIC

//
// End of File
//
