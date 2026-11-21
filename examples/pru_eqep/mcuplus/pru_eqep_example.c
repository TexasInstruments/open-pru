/*
*  Copyright (C) 2025 Texas Instruments Incorporated
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

// Include necessary header files
#include <kernel/dpl/DebugP.h>
#include <drivers/eqep.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <string.h>
// #include <drivers/pruicss/g_v0/cslr_icss_common.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/pruicss.h>
#include "eqep_diagnostic.h"
#include <pru0_load_bin.h>
#include <pru1_load_bin.h>
#include <rtupru0_load_bin.h>
#include <rtupru1_load_bin.h>
#include <txpru0_load_bin.h>
#include <txpru1_load_bin.h>

#define CTR_EN (1 << 3)
// Define offsets for each channel in DMEM
#define DMEM_CHANNEL_SIZE  0x400  // 4KB per channel
#define DMEM_CH0_OFFSET    0x0000  // Channel 0 starts at base
#define DMEM_CH1_OFFSET    0x400  // Channel 1 starts at base + 4KB
#define DMEM_CH2_OFFSET    0x800  // Channel 2 starts at base + 8KB
#define DMEM_CH3_OFFSET    0x0C00  // Channel 3 (RTU_PRU1) starts in PRU1's DRAM bank
#define DMEM_CH4_OFFSET    0x1000  // Channel 4 (PRU1) starts in PRU1's DRAM bank
#define DMEM_CH5_OFFSET    0x1400  // Channel 5 (TX_PRU1) starts in PRU1's DRAM bank

// Define position offsets for each channel (4 bytes each)
#define CH0_POSITION_OFFSET 0x1C     // Base starting offset
#define CH1_POSITION_OFFSET 0x20     // 0x1C + 0x4
#define CH2_POSITION_OFFSET 0x24     // 0x20 + 0x4
#define CH3_POSITION_OFFSET 0x28     // 0x24 + 0x4
#define CH4_POSITION_OFFSET 0x2C     // 0x28 + 0x4
#define CH5_POSITION_OFFSET 0x30     // 0x2C + 0x4

// Define phase-error counter offsets for each channel (PRU-write-only, R5F-read-only)
#define CH0_PHASE_ERR_OFFSET 0x34
#define CH1_PHASE_ERR_OFFSET 0x38
#define CH2_PHASE_ERR_OFFSET 0x3C
#define CH3_PHASE_ERR_OFFSET 0x40
#define CH4_PHASE_ERR_OFFSET 0x44
#define CH5_PHASE_ERR_OFFSET 0x48

// Define the delay after which we should print the results
#define PRINT_DELAY         1000000U

// Define to enable mem_limit ring-buffer-wrap debug prints (Ch5 bench test)
// #define TEST_MEM_LIMIT_DEBUG

// Array of offsets for easier access
static const uint32_t DMEM_OFFSETS[6] = {
    DMEM_CH0_OFFSET,
    DMEM_CH1_OFFSET,
    DMEM_CH2_OFFSET,
    DMEM_CH3_OFFSET,
    DMEM_CH4_OFFSET,
    DMEM_CH5_OFFSET
};

// Define constants
#define WRITE_PTR_OFFSET_MASK 0x0000FFFF
#define TIMESTAMP_MASK 0x00FFFFFF

// Handle for PRUICSS instance
PRUICSS_Handle gPruIcssXHandle;

// Pointer to PRU DRAM
void *gPru_dramx;
//void *gPru_dramx_0;
//void *gPru_dramx_1;
//void *gPru_dramx_2;
// Pointer to PRU configuration
static void *gPru_cfg;

ABZ_Handle ABZHandle[6];
ABZ_Config ABZInstance[6];
// Define macros
#define PRU_CLK_FREQ 300000000
#define EQEP_EDGE_THRESHOLD 4  /* edges to accumulate before processing — must match PRU firmware */

void EQEP_Get_Speed_ABZ(uint8_t channel);
void EQEP_get_speed_RT(uint8_t channel);
void EQEP_Get_position_ABZ(void);
void EQEP_PRU_clearPhaseErrorFlag(uint8_t channel);
void create_lut(void);  // Remove static keyword from the implementation
void ABZ_enable_load_share_mode(void *pruCfg, uint32_t pruSlice);
void EQEP_pruss_load_run_fw(void);
void ABZ_PRU_ICSS_Init(PRUICSS_Handle handle, ABZ_Handle ABZHandle, uint8_t pru_core);
void EQEP_diagnostic_main(void *args);

/**
 * Load and run firmware on specified PRU core
 *
 * @param handle: PRUICSS handle
 * @param pru_core: PRU core number
 * @param fw_image: Pointer to firmware image array
 * @param fw_size: Size of firmware image in bytes
 */
void EQEP_pruss_load_run_fw(void)
{
//PRU0 Cores
    // Disable all cores
    PRUICSS_disableCore(gPruIcssXHandle, PRUICSS_RTU_PRU0);    // ch0
    PRUICSS_disableCore(gPruIcssXHandle, PRUICSS_PRU0);        // ch1
    PRUICSS_disableCore(gPruIcssXHandle, PRUICSS_TX_PRU0);     // ch2

    /* Enable Load Share mode */
    gPru_cfg = (void *)(((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->cfgRegBase);
    ABZ_enable_load_share_mode(gPru_cfg, PRUICSS_PRU0);

    // Load firmware for each channel
    PRUICSS_writeMemory(gPruIcssXHandle, PRUICSS_IRAM_RTU_PRU(PRUICSS_PRU0),
        0, (uint32_t *) RTUPRU0Firmware_0,
        sizeof(RTUPRU0Firmware_0));

    PRUICSS_writeMemory(gPruIcssXHandle, PRUICSS_IRAM_PRU(PRUICSS_PRU0),
        0, (uint32_t *) PRU0Firmware_0,
        sizeof(PRU0Firmware_0));

    PRUICSS_writeMemory(gPruIcssXHandle, PRUICSS_IRAM_TX_PRU(PRUICSS_PRU0),
        0, (uint32_t *) TXPRU0Firmware_0,
        sizeof(TXPRU0Firmware_0));

    // Reset cores
    PRUICSS_resetCore(gPruIcssXHandle, PRUICSS_RTU_PRU0);
    PRUICSS_resetCore(gPruIcssXHandle, PRUICSS_PRU0);
    PRUICSS_resetCore(gPruIcssXHandle, PRUICSS_TX_PRU0);

    /* Run firmware */
    PRUICSS_enableCore(gPruIcssXHandle, PRUICSS_RTU_PRU0);
    PRUICSS_enableCore(gPruIcssXHandle, PRUICSS_PRU0);
    PRUICSS_enableCore(gPruIcssXHandle, PRUICSS_TX_PRU0);

//PRU1 Cores
// Disable all cores
   PRUICSS_disableCore(gPruIcssXHandle, PRUICSS_RTU_PRU1);    // ch0
   PRUICSS_disableCore(gPruIcssXHandle, PRUICSS_PRU1);        // ch1
   PRUICSS_disableCore(gPruIcssXHandle, PRUICSS_TX_PRU1);     // ch2

   /* Enable Load Share mode */
   gPru_cfg = (void *)(((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->cfgRegBase);
   ABZ_enable_load_share_mode(gPru_cfg, PRUICSS_PRU1);

   // Load firmware for each channel
   PRUICSS_writeMemory(gPruIcssXHandle, PRUICSS_IRAM_RTU_PRU(PRUICSS_PRU1),
       0, (uint32_t *) RTUPRU1Firmware_0,
       sizeof(RTUPRU1Firmware_0));

   PRUICSS_writeMemory(gPruIcssXHandle, PRUICSS_IRAM_PRU(PRUICSS_PRU1),
       0, (uint32_t *) PRU1Firmware_0,
       sizeof(PRU1Firmware_0));

   PRUICSS_writeMemory(gPruIcssXHandle, PRUICSS_IRAM_TX_PRU(PRUICSS_PRU1),
       0, (uint32_t *) TXPRU1Firmware_0,
       sizeof(TXPRU1Firmware_0));

   // Reset cores
   PRUICSS_resetCore(gPruIcssXHandle, PRUICSS_RTU_PRU1);
   PRUICSS_resetCore(gPruIcssXHandle, PRUICSS_PRU1);
   PRUICSS_resetCore(gPruIcssXHandle, PRUICSS_TX_PRU1);

   /* Run firmware */
   PRUICSS_enableCore(gPruIcssXHandle, PRUICSS_RTU_PRU1);
   PRUICSS_enableCore(gPruIcssXHandle, PRUICSS_PRU1);
   PRUICSS_enableCore(gPruIcssXHandle, PRUICSS_TX_PRU1);
}
// Function to initialize PRU ICSS
void ABZ_PRU_ICSS_Init(PRUICSS_Handle handle, ABZ_Handle ABZHandle, uint8_t pru_core)
{
    // Disable PRU core
    PRUICSS_disableCore(handle, pru_core);

    // Clear ICSS0 PRU data RAM
    gPru_dramx = (void *)((((PRUICSS_HwAttrs *)(handle->hwAttrs))->baseAddr) + PRUICSS_DATARAM(pru_core));
    memset(gPru_dramx, 0, (4 * 1024));

    // Initialize ICSS interrupt controller
    gPru_cfg = (void *)(((PRUICSS_HwAttrs *)(handle->hwAttrs))->cfgRegBase);
    PRUICSS_intcInit(handle, &icss0_intc_initdata);
}

int32_t idx=0;

// Main function
void pru_eqep_example_main(void *args)
{
    int32_t status;
    // Open drivers and board
    Drivers_open();
    status = Board_driversOpen();

    DebugP_assert(SystemP_SUCCESS == status);

    // Initialize PRU ICSS with single handle
    gPruIcssXHandle = PRUICSS_open(CONFIG_PRU_ICSS0);

    /*
    * Set the constant table C28 for TX_PRUs. Configure the constant table C28 to point to the TX cycle
    * counter. The counter is needed in firmware for adding waits and time-stamps.
    */
    PRUICSS_setConstantTblEntry(gPruIcssXHandle, PRUICSS_TX_PRU0, PRUICSS_CONST_TBL_ENTRY_C28, 0x250);
    PRUICSS_setConstantTblEntry(gPruIcssXHandle, PRUICSS_TX_PRU1, PRUICSS_CONST_TBL_ENTRY_C28, 0x258);

    for(int i=0; i<6; i++)
    {
        ABZHandle[i] = &ABZInstance[i];

        // Use single handle for all channel configurations
        ABZHandle[i]->baseMemAddr0 = (uint32_t *)(
            ((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->pru0DramBase +
            DMEM_OFFSETS[i]
        );
        ABZHandle[i]->baseMemAddr1 = (uint32_t *)(
            ((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->pru1DramBase +
            i*4
        );
        ABZHandle[i]->icssHandle = gPruIcssXHandle;
        ABZHandle[i]->prev_QPOS = 0;
        ABZHandle[i]->QPOSCOUNT = 0;
        ABZHandle[i]->read_ptr = ABZHandle[i]->baseMemAddr0;
        ABZHandle[i]->write_ptr = ABZHandle[i]->baseMemAddr1;
        ABZHandle[i]->mem_limit=DMEM_CHANNEL_SIZE;
        if(i<3) ABZHandle[i]->pru_slice=0;
        else ABZHandle[i]->pru_slice=1;
    }
    ABZHandle[0]->position_base = (void *)((uint32_t)(ABZHandle[0]->baseMemAddr1) + CH0_POSITION_OFFSET);
    ABZHandle[1]->position_base = (void *)((uint32_t)(ABZHandle[1]->baseMemAddr1) + CH0_POSITION_OFFSET);
    ABZHandle[2]->position_base = (void *)((uint32_t)(ABZHandle[2]->baseMemAddr1) + CH0_POSITION_OFFSET);
    ABZHandle[3]->position_base = (void *)((uint32_t)(ABZHandle[3]->baseMemAddr1) + CH0_POSITION_OFFSET);
    ABZHandle[4]->position_base = (void *)((uint32_t)(ABZHandle[4]->baseMemAddr1) + CH0_POSITION_OFFSET);
    ABZHandle[5]->position_base = (void *)((uint32_t)(ABZHandle[5]->baseMemAddr1) + CH0_POSITION_OFFSET);

    ABZHandle[0]->phase_err_base = (uint32_t *)((uint32_t)(ABZHandle[0]->baseMemAddr1) + CH0_PHASE_ERR_OFFSET);
    ABZHandle[1]->phase_err_base = (uint32_t *)((uint32_t)(ABZHandle[1]->baseMemAddr1) + CH0_PHASE_ERR_OFFSET);
    ABZHandle[2]->phase_err_base = (uint32_t *)((uint32_t)(ABZHandle[2]->baseMemAddr1) + CH0_PHASE_ERR_OFFSET);
    ABZHandle[3]->phase_err_base = (uint32_t *)((uint32_t)(ABZHandle[3]->baseMemAddr1) + CH0_PHASE_ERR_OFFSET);
    ABZHandle[4]->phase_err_base = (uint32_t *)((uint32_t)(ABZHandle[4]->baseMemAddr1) + CH0_PHASE_ERR_OFFSET);
    ABZHandle[5]->phase_err_base = (uint32_t *)((uint32_t)(ABZHandle[5]->baseMemAddr1) + CH0_PHASE_ERR_OFFSET);
    // Initialize PRU ICSS for each channel
    //PRU0 cores
    ABZ_PRU_ICSS_Init(gPruIcssXHandle, ABZHandle[0], PRUICSS_RTU_PRU0);
    ABZ_PRU_ICSS_Init(gPruIcssXHandle, ABZHandle[1], PRUICSS_PRU0);
    ABZ_PRU_ICSS_Init(gPruIcssXHandle, ABZHandle[2], PRUICSS_TX_PRU0);
    //PRU1 cores
    ABZ_PRU_ICSS_Init(gPruIcssXHandle, ABZHandle[3], PRUICSS_RTU_PRU1);
    ABZ_PRU_ICSS_Init(gPruIcssXHandle, ABZHandle[4], PRUICSS_PRU1);
    ABZ_PRU_ICSS_Init(gPruIcssXHandle, ABZHandle[5], PRUICSS_TX_PRU1);
    /* enable cycle counter */
    HW_WR_REG32((void *)((((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->baseAddr) + CSL_ICSS_G_PR1_RTU0_PR1_RTU0_IRAM_REGS_BASE), CTR_EN);   // RTU_PRU Core
    HW_WR_REG32((void *)((((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->baseAddr) + CSL_ICSS_G_PR1_PDSP0_IRAM_REGS_BASE), CTR_EN);
    HW_WR_REG32((void *)((((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->baseAddr) + CSL_ICSS_G_PR1_PDSP_TX0_IRAM_REGS_BASE), CTR_EN);        // TX_PRU Core

    HW_WR_REG32((void *)((((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->baseAddr) + CSL_ICSS_G_PR1_RTU1_PR1_RTU1_IRAM_REGS_BASE), CTR_EN);   // RTU_PRU Core
    HW_WR_REG32((void *)((((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->baseAddr) + CSL_ICSS_G_PR1_PDSP1_IRAM_REGS_BASE), CTR_EN);
    HW_WR_REG32((void *)((((PRUICSS_HwAttrs *)(gPruIcssXHandle->hwAttrs))->baseAddr) + CSL_ICSS_G_PR1_PDSP_TX1_IRAM_REGS_BASE), CTR_EN);        // TX_PRU Core

    create_lut();

    /* Startup sync: initialize read_ptr_offset to current write_ptr_offset so
     * stale edges accumulated before R5F init are not processed */
    for (int i = 0; i < 6; i++)
    {
        ABZHandle[i]->read_ptr_offset  = HW_RD_REG32(ABZHandle[i]->baseMemAddr1) & WRITE_PTR_OFFSET_MASK;
        ABZHandle[i]->write_ptr_offset = ABZHandle[i]->read_ptr_offset;
    }
    // Load and run firmware
    EQEP_pruss_load_run_fw();


    // Log messages
    DebugP_log("\r\n ABZ setup finished\n");
    DebugP_log("EQEP Position Speed Test Started ...\r\n");

    uint64_t last_print_us = ClockP_getTimeUsec();

    // Main polling loop
    while (1)
    {
        for (int ch = 0; ch < 6; ch++)
        {
            uint32_t curr_write_ptr_offset = HW_RD_REG32(ABZHandle[ch]->baseMemAddr1) & WRITE_PTR_OFFSET_MASK;

            /* Calculate edges accumulated since last processed (handles ring buffer wraparound) */
            uint32_t edge_delta;
            if (curr_write_ptr_offset >= ABZHandle[ch]->read_ptr_offset)
                edge_delta = (curr_write_ptr_offset - ABZHandle[ch]->read_ptr_offset) / 4;
            else
            {
                edge_delta = (ABZHandle[ch]->mem_limit - ABZHandle[ch]->read_ptr_offset + curr_write_ptr_offset) / 4;
#ifdef TEST_MEM_LIMIT_DEBUG
                DebugP_log("\r\n[MEM_LIMIT BUG] Ch%d wrap: mem_limit=0x%x (should be 0x%x) "
                    "read_ptr_offset=0x%x write_ptr_offset=0x%x -> edge_delta=%d\n",
                    ch, ABZHandle[ch]->mem_limit, DMEM_CHANNEL_SIZE,
                    ABZHandle[ch]->read_ptr_offset, curr_write_ptr_offset, edge_delta);
#endif
            }

            /* If no change in edge delta, then set the direction to 0 -> no change */
            ABZHandle[ch]->direction = 0;

            /* Phase error shadow-compare: PRU increments its counter on invalid
             * A/B transitions; R5F only ever reads it and diffs against its own
             * last-seen value. Checked every pass, not gated by edge_delta, so
             * a phase error is never missed even if position/speed processing
             * hasn't hit threshold yet. */
            uint32_t curr_phase_err_count = HW_RD_REG32((uint32_t)ABZHandle[ch]->phase_err_base);
            if (curr_phase_err_count != ABZHandle[ch]->phase_err_count_last_seen)
            {
                ABZHandle[ch]->phase_error_flag = 1;
                ABZHandle[ch]->phase_err_count_last_seen = curr_phase_err_count;
            }
            ABZHandle[ch]->phase_err_count = curr_phase_err_count;

            /* QPOS/direction read: same ungated-every-pass treatment as the
             * phase-error read above. QPOS is a point-in-time snapshot with
             * no batching requirement of its own — gating it behind
             * EQEP_EDGE_THRESHOLD (as before) meant a trailing sub-threshold
             * edge_delta at idle never got picked up, unlike interrupt-mode
             * (eqep_diagnostic.c), which reads QPOS on every ISR entry with
             * no equivalent gate. That gating mismatch was the source of the
             * position-count divergence between the two modes. */
            ABZHandle[ch]->prev_QPOS  = ABZHandle[ch]->QPOSCOUNT;
            ABZHandle[ch]->QPOSCOUNT  = HW_RD_REG32((uint32_t)ABZHandle[ch]->position_base);

            /* QPOS is a free-running uint32_t in PRU firmware (plain add/sub,
             * no QPOSMAX ceiling), so it wraps 0xFFFFFFFF -> 0 (forward) or
             * 0 -> 0xFFFFFFFF (reverse). A direct unsigned compare of
             * QPOSCOUNT vs prev_QPOS breaks exactly at that wrap: e.g.
             * prev=0xFFFFFFFF, curr=0x00000000 is still forward motion, but
             * curr < prev as raw unsigned values, so the old ">"/"<" logic
             * would report it as reverse.
             *
             * Fix: subtract as unsigned (which wraps modulo 2^32, exactly
             * undoing the counter's own wraparound) and reinterpret the
             * result as signed. Since we poll fast relative to the encoder
             * rate, the true step between polls is always tiny compared to
             * 2^31, so the sign of the reinterpreted result always matches
             * the true direction, wrap or no wrap. */
            int32_t qpos_diff = (int32_t)(ABZHandle[ch]->QPOSCOUNT - ABZHandle[ch]->prev_QPOS);
            if      (qpos_diff > 0) ABZHandle[ch]->direction =  1;
            else if (qpos_diff < 0) ABZHandle[ch]->direction = -1;

            if (edge_delta >= EQEP_EDGE_THRESHOLD)
            {
                ABZHandle[ch]->write_ptr_offset = curr_write_ptr_offset;
                EQEP_Get_Speed_ABZ(ch);
            }
        }

        /* Print status every 1 second 
        this is just for calculating that atleast 1(PRINT_DELAY) second has passed since the last print */
        uint64_t now_us = ClockP_getTimeUsec();
        if ((now_us - last_print_us) >= PRINT_DELAY)
        {
            DebugP_log("\r\nSpeeds (Hz): Ch0=%d Ch1=%d Ch2=%d Ch3=%d Ch4=%d Ch5=%d\n",
                ABZHandle[0]->speed, ABZHandle[1]->speed, ABZHandle[2]->speed,
                ABZHandle[3]->speed, ABZHandle[4]->speed, ABZHandle[5]->speed);
            DebugP_log("  Positions: Ch0=0x%x Ch1=0x%x Ch2=0x%x Ch3=0x%x Ch4=0x%x Ch5=0x%x\n",
                ABZHandle[0]->QPOSCOUNT, ABZHandle[1]->QPOSCOUNT, ABZHandle[2]->QPOSCOUNT,
                ABZHandle[3]->QPOSCOUNT, ABZHandle[4]->QPOSCOUNT, ABZHandle[5]->QPOSCOUNT);
            DebugP_log("  Directions: Ch0=%d Ch1=%d Ch2=%d Ch3=%d Ch4=%d Ch5=%d\n",
                ABZHandle[0]->direction, ABZHandle[1]->direction, ABZHandle[2]->direction,
                ABZHandle[3]->direction, ABZHandle[4]->direction, ABZHandle[5]->direction);
            DebugP_log("  PhaseErr: Ch0=%d Ch1=%d Ch2=%d Ch3=%d Ch4=%d Ch5=%d\n",
                ABZHandle[0]->phase_error_flag, ABZHandle[1]->phase_error_flag, ABZHandle[2]->phase_error_flag,
                ABZHandle[3]->phase_error_flag, ABZHandle[4]->phase_error_flag, ABZHandle[5]->phase_error_flag);
            last_print_us = now_us;
        }
    }



    // Close drivers and board
    Board_driversClose();
    Drivers_close();
}

// Function to get speed and position of ABZ
void EQEP_Get_Speed_ABZ(uint8_t channel)
{
    // Calculate read pointer and write pointer
    ABZHandle[channel]->read_ptr = (ABZHandle[channel]->baseMemAddr0) + (ABZHandle[channel]->read_ptr_offset) / 4;
    ABZHandle[channel]->write_ptr_offset = HW_RD_REG32(ABZHandle[channel]->baseMemAddr1) & WRITE_PTR_OFFSET_MASK;
    ABZHandle[channel]->write_ptr = (ABZHandle[channel]->baseMemAddr0) + (ABZHandle[channel]->write_ptr_offset) / 4;

    // Get timestamps
    ABZHandle[channel]->prev_ts = HW_RD_REG32(ABZHandle[channel]->read_ptr) & TIMESTAMP_MASK;
    ABZHandle[channel]->cur_ts = HW_RD_REG32(ABZHandle[channel]->write_ptr) & TIMESTAMP_MASK;

    // Calculate speed and position
    if (ABZHandle[channel]->read_ptr_offset > ABZHandle[channel]->write_ptr_offset)
    {
        // Calculate delta time and edges
        if (ABZHandle[channel]->prev_ts > ABZHandle[channel]->cur_ts)
            ABZHandle[channel]->delta_t = TIMESTAMP_MASK - ABZHandle[channel]->prev_ts + ABZHandle[channel]->cur_ts;
        else
            ABZHandle[channel]->delta_t = ABZHandle[channel]->cur_ts - ABZHandle[channel]->prev_ts;

        ABZHandle[channel]->edges = (ABZHandle[channel]->mem_limit - ABZHandle[channel]->read_ptr_offset + ABZHandle[channel]->write_ptr_offset) / 4;

        // Calculate speed
        ABZHandle[channel]->speed = ABZHandle[channel]->edges * ((PRU_CLK_FREQ) / (ABZHandle[channel]->delta_t));

#ifdef TEST_MEM_LIMIT_DEBUG
        {
            uint32_t correct_edges = (DMEM_CHANNEL_SIZE - ABZHandle[channel]->read_ptr_offset + ABZHandle[channel]->write_ptr_offset) / 4;
            uint32_t correct_speed = correct_edges * ((PRU_CLK_FREQ) / (ABZHandle[channel]->delta_t));
            DebugP_log("\r\n[MEM_LIMIT BUG] Ch%d speed-calc wrap: mem_limit=0x%x (should be 0x%x) "
                "read_ptr_offset=0x%x write_ptr_offset=0x%x delta_t=%d -> edges=%d speed=%d "
                "(correct: edges=%d speed=%d)\n",
                channel, ABZHandle[channel]->mem_limit, DMEM_CHANNEL_SIZE,
                ABZHandle[channel]->read_ptr_offset, ABZHandle[channel]->write_ptr_offset,
                ABZHandle[channel]->delta_t, ABZHandle[channel]->edges, ABZHandle[channel]->speed,
                correct_edges, correct_speed);
        }
#endif
    }
    else
    {
        // Similar calculations for the else case
        if (ABZHandle[channel]->prev_ts > ABZHandle[channel]->cur_ts)
            ABZHandle[channel]->delta_t = TIMESTAMP_MASK - ABZHandle[channel]->prev_ts + ABZHandle[channel]->cur_ts;
        else
            ABZHandle[channel]->delta_t = ABZHandle[channel]->cur_ts - ABZHandle[channel]->prev_ts;

        ABZHandle[channel]->edges = (ABZHandle[channel]->write_ptr_offset - ABZHandle[channel]->read_ptr_offset) / 4;
        ABZHandle[channel]->speed = ABZHandle[channel]->edges * ((PRU_CLK_FREQ) / (ABZHandle[channel]->delta_t));
    }


    // Update pointers and counters
    ABZHandle[channel]->prev_ts = ABZHandle[channel]->cur_ts;
    ABZHandle[channel]->read_ptr = ABZHandle[channel]->write_ptr;
    ABZHandle[channel]->read_ptr_offset = ABZHandle[channel]->write_ptr_offset;
    ABZHandle[channel]->iter++;
}

void create_lut(void)
{
    void *a_b_transition_base = (void *)((uint32_t)(ABZHandle[0]->baseMemAddr1) + 0xf0);

    // STATE_00_00 (0000)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x0, 0);  // No change
    // STATE_00_01 (0001)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x1, 2); // Decrement
    // STATE_00_10 (0010)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x2, 1);  // Increment
    // STATE_00_11 (0011) - diagonal transition (SPRU790D Fig. 6: 00<->11 is invalid) -> phase error
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x3, 3);  // Phase error
    // STATE_01_00 (0100)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x4, 1);  // Increment
    // STATE_01_01 (0101)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x5, 3);  // Invalid state (phase error)
    // STATE_01_10 (0110) - diagonal transition (SPRU790D Fig. 6: 01<->10 is invalid) -> phase error
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x6, 3);  // Phase error
    // STATE_01_11 (0111)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x7, 2); // Decrement
    // STATE_10_00 (1000)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x8, 2); // Decrement
    // STATE_10_01 (1001) - diagonal transition (SPRU790D Fig. 6: 10<->01 is invalid) -> phase error
    HW_WR_REG8((uint32_t)a_b_transition_base + 0x9, 3);  // Phase error
    // STATE_10_10 (1010)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0xA, 3);  // Invalid state (phase error)
    // STATE_10_11 (1011)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0xB, 1);  // Increment
    // STATE_11_00 (1100) - diagonal transition (SPRU790D Fig. 6: 11<->00 is invalid) -> phase error
    HW_WR_REG8((uint32_t)a_b_transition_base + 0xC, 3);  // Phase error
    // STATE_11_01 (1101)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0xD, 1);  // Increment
    // STATE_11_10 (1110)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0xE, 2); // Decrement
    // STATE_11_11 (1111)
    HW_WR_REG8((uint32_t)a_b_transition_base + 0xF, 0);  // No change
}

void ABZ_enable_load_share_mode(void *pruCfg, uint32_t pruSlice)
{
    uint32_t regVal;
    if(pruSlice == 1)
    {
        regVal = HW_RD_REG32((uint8_t *)pruCfg + CSL_ICSSCFG_EDPRU1TXCFGREGISTER);
        regVal |= CSL_ICSSCFG_EDPRU1TXCFGREGISTER_PRU1_ENDAT_SHARE_EN_MASK;
        HW_WR_REG32((uint8_t *)pruCfg + CSL_ICSSCFG_EDPRU1TXCFGREGISTER, regVal);
    }
    else
    {
        regVal = HW_RD_REG32((uint8_t *)pruCfg + CSL_ICSSCFG_EDPRU0TXCFGREGISTER);
        regVal |= CSL_ICSSCFG_EDPRU0TXCFGREGISTER_PRU0_ENDAT_SHARE_EN_MASK;
        HW_WR_REG32((uint8_t *)pruCfg + CSL_ICSSCFG_EDPRU0TXCFGREGISTER, regVal);
    }
}
void EQEP_Get_position_ABZ(void)
{
    // Update position and direction
    for(int channel=0;channel<6;channel++) ABZHandle[channel]->prev_QPOS = ABZHandle[channel]->QPOSCOUNT;
    for(int channel=0;channel<6;channel++)
    {
        ABZHandle[channel]->QPOSCOUNT = HW_RD_REG32((uint32_t)ABZHandle[channel]->position_base);
        ABZHandle[0]->position_buffer[idx]=ABZHandle[channel]->QPOSCOUNT;
        idx++;
    }
        for(int channel=0;channel<6;channel++)
        {
            /* See the wraparound-safe signed-diff note in the main polling
             * loop above (pru_eqep_example_main) — same QPOS free-running
             * uint32_t wrap issue applies here. */
            int32_t qpos_diff = (int32_t)(ABZHandle[channel]->QPOSCOUNT - ABZHandle[channel]->prev_QPOS);
            if      (qpos_diff > 0) ABZHandle[channel]->direction =  1;
            else if (qpos_diff < 0) ABZHandle[channel]->direction = -1;
            else                    ABZHandle[channel]->direction =  0;
        }
   if(idx>=950) idx=0;
}
void EQEP_PRU_clearPhaseErrorFlag(uint8_t channel)
{
    // Only clears the R5F-local flag; never touches PRU/DMEM (single-owner rule)
    ABZHandle[channel]->phase_error_flag = 0;
}

void EQEP_get_speed_RT(uint8_t channel)
{

    // Calculate speed
    ABZHandle[channel]->write_ptr_offset = HW_RD_REG32(ABZHandle[channel]->baseMemAddr1) & WRITE_PTR_OFFSET_MASK;
    ABZHandle[channel]->write_ptr = (ABZHandle[channel]->baseMemAddr0) + (ABZHandle[channel]->write_ptr_offset) / 4;
    ABZHandle[channel]->delta_t = HW_RD_REG32(ABZHandle[channel]->write_ptr) & TIMESTAMP_MASK;
    ABZHandle[channel]->speed = ((PRU_CLK_FREQ) / (ABZHandle[channel]->delta_t));


}
