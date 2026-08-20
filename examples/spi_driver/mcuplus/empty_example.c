/*
 * Copyright (C) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PRU-SPI encoder-emulation demo - R5F side (phase 3, MODE3/LSB only)
 *
 * Loads PRU1 (slave responder) then PRU0 (master), both persistent — neither
 * halts after a burst. R5F sets SPI config once before loading, then
 * repeatedly: prompts for 5 independent 16-bit command words, writes all 5 +
 * sets cfg_trigger once, polls for PRU0 to clear cfg_trigger after running
 * all 5 transfers back-to-back within one CS-low burst, and prints the 5
 * responses. PRU1 always responds with 5 fixed words (0x1111/0x2222/0x3333/
 * 0x4444/0x5555) regardless of the command content - no decode.
 *
 * PRU_SPI_runTransaction (single-command) is kept below for the other 7
 * (MODE x bit-order) firmware paths, which are unchanged from phase 2.
 *
 * PRU0 DMEM0 (ICSS_M1 absolute base 0x48600000):
 *   0x00        cfg_mode      R5F writes PRU_SPI_Mode before loading PRU0
 *   0x04        cfg_bitorder  R5F writes PRU_SPI_BitOrder before loading PRU0
 *   0x08        master_rx     legacy single-transfer rx slot (MODE0-2, MODE3/MSB)
 *   0x0C        cfg_command   legacy single-transfer tx slot (same paths)
 *   0x10        cfg_trigger   R5F sets to 1 to start; PRU0 clears to 0 when done
 *   0x14-0x24   cfg_cmd0..4     5 command words (MODE3/LSB path only)
 *   0x28-0x38   master_rx5_0..4 5 response words (MODE3/LSB path only)
 *
 * PRU1 DMEM1 (ICSS_M1 absolute base 0x48602000):
 *   0x00  slave_rx     legacy single-transfer debug slot (unused by MODE3/LSB path)
 *
 * PRU1 is hardcoded to MODE3/LSB/16-bit and to 5 fixed responses -
 * it is not part of the configurable driver API.
 */

#include <stdio.h>
#include <stdint.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/ClockP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/pruicss.h>

#include <pru0_load_bin.h>
#include <pru1_load_bin.h>

#define PRU0_DMEM_BASE      (0x48600000U)
#define DMEM_CFG_MODE       (0x00U)
#define DMEM_CFG_BITORDER   (0x04U)
#define DMEM_MASTER_RX      (0x08U)
#define DMEM_CFG_COMMAND    (0x0CU)
#define DMEM_CFG_TRIGGER    (0x10U)

/* 5x16-bit command/response DMEM slots, MODE3/LSB path only (phase 3) */
#define DMEM_CFG_CMD0       (0x14U)
#define DMEM_CFG_CMD1       (0x18U)
#define DMEM_CFG_CMD2       (0x1CU)
#define DMEM_CFG_CMD3       (0x20U)
#define DMEM_CFG_CMD4       (0x24U)
#define DMEM_MASTER_RX5_0   (0x28U)
#define DMEM_MASTER_RX5_1   (0x2CU)
#define DMEM_MASTER_RX5_2   (0x30U)
#define DMEM_MASTER_RX5_3   (0x34U)
#define DMEM_MASTER_RX5_4   (0x38U)

#define PRU1_DMEM_BASE      (0x48602000U)
#define DMEM_SLAVE_RX       (0x00U)

/* Must match PRU1's CMD0-3 constants in main.asm (fixed at build time) */
#define CMD_DATA_ID_0       (0x1U)
#define CMD_DATA_ID_1       (0x2U)
#define CMD_DATA_ID_2       (0x3U)
#define CMD_DATA_ID_3       (0x4U)

#define TRIGGER_POLL_TIMEOUT_US (5000U)
#define TRIGGER_POLL_SLEEP_US   (10U)

typedef enum
{
    PRU_SPI_MODE_0 = 0,
    PRU_SPI_MODE_1,
    PRU_SPI_MODE_2,
    PRU_SPI_MODE_3
} PRU_SPI_Mode;

typedef enum
{
    PRU_SPI_MSB_FIRST = 0,
    PRU_SPI_LSB_FIRST
} PRU_SPI_BitOrder;

typedef struct
{
    PRU_SPI_Mode     mode;
    PRU_SPI_BitOrder bitOrder;
} PRU_SPI_Config;

static PRUICSS_Handle gPruHandle;

static inline void dmem_write32(uint32_t base, uint32_t offset, uint32_t value)
{
    volatile uint32_t *p = (volatile uint32_t *)(base + offset);
    *p = value;
}

static inline uint32_t dmem_read32(uint32_t base, uint32_t offset)
{
    volatile uint32_t *p = (volatile uint32_t *)(base + offset);
    return *p;
}

/* Must be called BEFORE PRUICSS_loadFirmware(PRU0) - loadFirmware resets and
 * starts the core immediately, so config has to already be in DMEM0. */
static void PRU_SPI_setConfig(const PRU_SPI_Config *cfg)
{
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_MODE,     (uint32_t)cfg->mode);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_BITORDER, (uint32_t)cfg->bitOrder);
}

/* Writes the command word, sets cfg_trigger, and polls until PRU0 clears it.
 * Returns 1 on success, 0 on timeout. */
static int PRU_SPI_runTransaction(uint32_t commandWord, uint32_t *responseOut)
{
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_COMMAND, commandWord);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_TRIGGER, 1U);

    uint32_t elapsed = 0U;
    while (dmem_read32(PRU0_DMEM_BASE, DMEM_CFG_TRIGGER) != 0U)
    {
        ClockP_usleep(TRIGGER_POLL_SLEEP_US);
        elapsed += TRIGGER_POLL_SLEEP_US;
        if (elapsed >= TRIGGER_POLL_TIMEOUT_US)
        {
            return 0;
        }
    }

    *responseOut = dmem_read32(PRU0_DMEM_BASE, DMEM_MASTER_RX);
    return 1;
}

/* MODE3/LSB-only path (phase 3): writes 5 independent 16-bit command words,
 * sets cfg_trigger once, polls until PRU0 clears it after running all 5
 * transfers back-to-back, then reads 5 independent response words.
 * commandWords/responsesOut must each point to 5 uint32_t. Returns 1 on
 * success, 0 on timeout. */
static int PRU_SPI_runTransaction5(const uint32_t *commandWords, uint32_t *responsesOut)
{
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_CMD0, commandWords[0]);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_CMD1, commandWords[1]);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_CMD2, commandWords[2]);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_CMD3, commandWords[3]);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_CMD4, commandWords[4]);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_TRIGGER, 1U);

    uint32_t elapsed = 0U;
    while (dmem_read32(PRU0_DMEM_BASE, DMEM_CFG_TRIGGER) != 0U)
    {
        ClockP_usleep(TRIGGER_POLL_SLEEP_US);
        elapsed += TRIGGER_POLL_SLEEP_US;
        if (elapsed >= TRIGGER_POLL_TIMEOUT_US)
        {
            return 0;
        }
    }

    responsesOut[0] = dmem_read32(PRU0_DMEM_BASE, DMEM_MASTER_RX5_0);
    responsesOut[1] = dmem_read32(PRU0_DMEM_BASE, DMEM_MASTER_RX5_1);
    responsesOut[2] = dmem_read32(PRU0_DMEM_BASE, DMEM_MASTER_RX5_2);
    responsesOut[3] = dmem_read32(PRU0_DMEM_BASE, DMEM_MASTER_RX5_3);
    responsesOut[4] = dmem_read32(PRU0_DMEM_BASE, DMEM_MASTER_RX5_4);
    return 1;
}

void empty_example_main(void *args)
{
    int32_t  status;
    uint32_t input;

    Drivers_open();
    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);

    gPruHandle = PRUICSS_open(CONFIG_PRU_ICSS0);
    DebugP_assert(gPruHandle != NULL);

    status = PRUICSS_initMemory(gPruHandle, PRUICSS_DATARAM(PRUICSS_PRU0));
    DebugP_assert(status != 0);
    status = PRUICSS_initMemory(gPruHandle, PRUICSS_DATARAM(PRUICSS_PRU1));
    DebugP_assert(status != 0);

    /* PRU1 (slave responder) is hardcoded to MODE3/MSB - match it here.
     * Must be written before PRU0 loads. Trigger starts cleared. */
    PRU_SPI_Config cfg = { .mode = PRU_SPI_MODE_3, .bitOrder = PRU_SPI_MSB_FIRST };
    PRU_SPI_setConfig(&cfg);
    dmem_write32(PRU0_DMEM_BASE, DMEM_CFG_TRIGGER, 0U);

    status = PRUICSS_loadFirmware(gPruHandle, PRUICSS_PRU1,
                                  PRU1Firmware_0, sizeof(PRU1Firmware_0));
    DebugP_assert(SystemP_SUCCESS == status);
    DebugP_log("[PRU-SPI] PRU1 slave responder loaded\r\n");
    status = PRUICSS_loadFirmware(gPruHandle, PRUICSS_PRU0,
                                  PRU0Firmware_0, sizeof(PRU0Firmware_0));
    DebugP_assert(SystemP_SUCCESS == status);
    DebugP_log("[PRU-SPI] PRU0 master loaded (persistent, never halts)\r\n");

    DebugP_log("Enter 5 16-bit command words per round; PRU1 always responds\r\n");
    DebugP_log("with 0x1111, 0x2222, 0x3333, 0x4444, 0x5555 (in order).\r\n");
    while (1)
    {
        uint32_t commandWords[5];
        uint32_t responses[5];
        uint32_t i;

        DebugP_log("\r\nEnter 5 16-bit command words, one at a time:\r\n");
        i = 0U;
        while (i < 5U)
        {
            DebugP_log("  cmd[%u]: ", i);
            DebugP_scanf("%x", &input);

            if (input > 0xFFFFU)
            {
                DebugP_log("Invalid input, must fit in 16 bits (0-65535).\r\n");
                continue;
            }

            commandWords[i] = input;
            i++;
        }

        if (!PRU_SPI_runTransaction5(commandWords, responses))
        {
            DebugP_log("ERROR: PRU0 timeout waiting for trigger clear.\r\n");
            continue;
        }

        for (i = 0U; i < 5U; i++)
        {
            DebugP_log("[PRU-SPI] sent cmd[%u]=0x%04X  response[%u]=0x%04X\r\n",
                       i, commandWords[i], i, responses[i]);
        }
    }

    PRUICSS_disableCore(gPruHandle, PRUICSS_PRU0);
    PRUICSS_disableCore(gPruHandle, PRUICSS_PRU1);

    Board_driversClose();
    Drivers_close();
}
