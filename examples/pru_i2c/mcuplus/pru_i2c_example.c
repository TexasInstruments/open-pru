/*
 *  Copyright (C) 2026 Texas Instruments Incorporated
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/ClockP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_config.h"
#include "ti_board_open_close.h"
#include <drivers/pruicss.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/hw_include/cslr_soc.h>

/* PRU <-> host memory map and command/response codes (shared with the PRU
 * firmware, do not duplicate the values here) */
#include "pru_i2c_interface.h"

/*
 *  This example shows an R5F core (NORTOS) using the PRU I2C master firmware
 *  from examples/pru_i2c/firmware to talk to an I2C target:
 *   1. the I2C configuration is written into the PRU data RAM
 *   2. the PRU firmware is loaded and started
 *   3. the R5F sends commands to the PRU through the data RAM and waits for
 *      the PRU to report completion
 *
 *  The demo transfers are written for a generic EEPROM-style target (for
 *  example an AT24C02 / 24LC02) that takes a one byte register pointer
 *  followed by data bytes:
 *   - write: [pointer, data0 ... dataN-1]
 *   - read back: write [pointer], then read N bytes
 */

/* ========================================================================== */
/*                         User configurable settings                         */
/* ========================================================================== */

/* PRU core that runs the I2C firmware: 0 = ICSSM0 PRU0, 1 = ICSSM0 PRU1.
 *
 * AM261x-LP: the BoosterPack headers carry PR0_PRU1_GPIO17..19 but none of the
 * PR0_PRU0_GPIOx signals (those are used by the on-board Ethernet PHY), so
 * the default is PRU1. If you change this, update the pinmux in example.syscfg
 * to the matching PR0_PRUx_GPIOy pins as well. */
#define PRU_I2C_PRU_CORE        (1U)

/* PRU GPIO numbers used for the bus. The firmware uses one number per signal
 * for both the output (R30[n]) and the input (R31[n]) register bit, so the
 * pad has to be PR0_PRUx_GPIO<n> in example.syscfg. You can change these two
 * numbers, but keep them in sync with example.syscfg.
 *   SCL: PR0_PRU1_GPIO18 (pad GPIO120, BoosterPack J1.4)
 *   SDA: PR0_PRU1_GPIO19 (pad GPIO119, BoosterPack J1.3) */
#define PRU_I2C_SCL_PIN         (18U)
#define PRU_I2C_SDA_PIN         (19U)

/* I2C bus frequency. PRU_I2C_BUS_FREQ is the firmware's frequency selector
 * (ICSS_I2C_100KHZ_FREQ, ICSS_I2C_400KHZ_FREQ or ICSS_I2C_1MHZ_FREQ). The
 * actual SCL rate is set by the IEP compare increment below. */
#define PRU_I2C_BUS_FREQ        (ICSS_I2C_400KHZ_FREQ)
#define PRU_I2C_BUS_HZ          (400000U)

/* The firmware advances its state machine once per IEP compare event and
 * needs 4 ticks per SCL period, with SCL low for 2 of them. The IEP counts
 * PRU_I2C_IEP_DEFAULT_INC per IEP clock, and its clock comes from SysConfig
 * (250 MHz by default on AM261x, while the IEP_CMP_INCREMENT_VAL_* constants
 * in pru_i2c_interface.h assume 200 MHz). So derive the increment from the
 * configured IEP clock: the tick is at least 1/(4 * PRU_I2C_BUS_HZ), and at
 * least PRU_I2C_MIN_TICK_NS so that SCL low meets the Fast-mode
 * tLOW >= 1.3 us (with margin for the few cycles the edges move by). At
 * 400 kHz the second limit wins: about 368 kHz. */
#define PRU_I2C_IEP_DEFAULT_INC (5U)
#define PRU_I2C_MIN_TICK_NS     (680U)
#define PRU_I2C_IEP_CLKS_BUS    \
    ((CONFIG_PRU_ICSS0_IEP_CLK_FREQ_HZ + (4U * PRU_I2C_BUS_HZ) - 1U) / (4U * PRU_I2C_BUS_HZ))
#define PRU_I2C_IEP_CLKS_TLOW   \
    (((CONFIG_PRU_ICSS0_IEP_CLK_FREQ_HZ / 1000U) * PRU_I2C_MIN_TICK_NS + 999999U) / 1000000U)
#define PRU_I2C_IEP_CLKS_PER_TICK \
    ((PRU_I2C_IEP_CLKS_BUS > PRU_I2C_IEP_CLKS_TLOW) ? PRU_I2C_IEP_CLKS_BUS : PRU_I2C_IEP_CLKS_TLOW)
#define PRU_I2C_IEP_INCREMENT   (PRU_I2C_IEP_CLKS_PER_TICK * PRU_I2C_IEP_DEFAULT_INC)

/* How long a target may hold SCL low (clock stretching) before the transfer
 * ends with TIME_OUT_ERROR, in state machine ticks: the SMBus tTIMEOUT of
 * 25 ms. */
#define PRU_I2C_SCL_TIMEOUT_TICKS \
    ((25000U * 1000U) / ((PRU_I2C_IEP_CLKS_PER_TICK * 1000000U) / (CONFIG_PRU_ICSS0_IEP_CLK_FREQ_HZ / 1000U)))

/* 7-bit address of the I2C target used for the demo transfers */
#define PRU_I2C_TARGET_ADDR     (0x50U)
/* Register pointer (first byte of every demo transfer) */
#define PRU_I2C_TARGET_REG      (0x00U)
/* Number of data bytes written and read back */
#define PRU_I2C_TEST_LEN        (4U)
/* Time to wait after a write before the target responds again. EEPROMs hold
 * off for the internal write cycle (typically 5 ms max). */
#define PRU_I2C_WRITE_CYCLE_US  (10000U)

/* How long the R5F waits for the PRU to report a response */
#define PRU_I2C_TIMEOUT_US      (100000U)

/* ========================================================================== */
/*                                Local macros                                */
/* ========================================================================== */

/* PRU firmware image array. The header is generated by the PRU firmware build
 * into examples/pru_i2c/firmware/am261x-lp/ */
#if (PRU_I2C_PRU_CORE == 0U)
#include <pru0_load_bin.h>
#define PRU_I2C_FIRMWARE        PRU0Firmware_0
#define PRU_I2C_OUT_CTRL_REG    (CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_ICSSM0_PRU0_GPIO_OUT_CTRL)
#else
#include <pru1_load_bin.h>
#define PRU_I2C_FIRMWARE        PRU1Firmware_0
#define PRU_I2C_OUT_CTRL_REG    (CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_ICSSM0_PRU1_GPIO_OUT_CTRL)
#endif

/* Global words, shared by both PRU cores. The firmware reads them through
 * ICSS_DMEM0_CONST, which is DMEM0 on PRU0 and on PRU1 alike, so they always
 * live in PRU0's data RAM. The layout is defined by pru_i2c_interface.h. */
#define PRU_I2C_IRQ_REG         (IRQ_COMMON_REGISTER_OFFSET)            /* u16: bit n set by PRU, cleared by R5F */
#define PRU_I2C_FREQ_REG        (I2C_BUS_FREQUENCY_OFFSET)              /* u16: frequency selection */
#define PRU_I2C_IEP_INC_REG     (I2C_BUS_FREQUENCY_OFFSET + 2U)         /* u16: IEP compare increment */

/* Instance 0 registers, in the data RAM of the core that runs the firmware
 * (PRU0 -> DMEM0, PRU1 -> DMEM1). ICSS_I2C_COMMAND_OFFSET is a 32-bit word: the low
 * half is the response of the PRU, the high half is the command from the R5F. */
#define PRU_I2C_RESPONSE_REG    (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_COMMAND_OFFSET)
#define PRU_I2C_COMMAND_REG     (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_COMMAND_OFFSET + 2U)
#define PRU_I2C_TX_FIFO_REG     (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_BUF_OFFSET)          /* u8 */
#define PRU_I2C_RX_FIFO_REG     (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_BUF_OFFSET + 1U)     /* u8 */
#define PRU_I2C_COUNT_REG       (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_CNT_OFFSET)          /* u16 */
#define PRU_I2C_CON_REG         (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_CON_OFFSET)          /* u32 */
#define PRU_I2C_SA_REG          (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_SA_OFFSET)           /* u16 */
#define PRU_I2C_SCL_PIN_REG     (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_PRU_PIN_OFFSET)      /* u8 */
#define PRU_I2C_SDA_PIN_REG     (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_PRU_PIN_OFFSET + 1U) /* u8 */
#define PRU_I2C_INST_ID_REG     (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_PRU_INST_ID_OFFSET)  /* u8 */
#define PRU_I2C_SCL_TIMEOUT_REG (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_SCL_TIMEOUT_OFFSET)  /* u16 */

/* Bit of the shared IRQ register that belongs to this instance. Use the core
 * number so a PRU0 and a PRU1 instance can run side by side. */
#define PRU_I2C_INST_ID         (PRU_I2C_PRU_CORE)

/* The firmware accepts a data count of 1..255 */
#define PRU_I2C_MAX_COUNT       (255U)

/* ========================================================================== */
/*                             Global variables                               */
/* ========================================================================== */

PRUICSS_Handle gPruIcss0Handle;

/* Base address of the data RAM of the PRU that runs the I2C firmware */
static uintptr_t gPruDmemBase;
/* Base address of DMEM0, which holds the global IRQ and frequency words */
static uintptr_t gPruGlobalBase;

/* ========================================================================== */
/*                          Local function definitions                        */
/* ========================================================================== */

static const char *PruI2c_responseToString(uint16_t response)
{
    const char *str;

    switch (response)
    {
        case COMMAND_SUCCESS:             str = "COMMAND_SUCCESS"; break;
        case RESET_COMMAND_FAILED:        str = "RESET_COMMAND_FAILED"; break;
        case SETUP_COMMAND_FAILED:        str = "SETUP_COMMAND_FAILED"; break;
        case TX_COMMAND_FAILED:           str = "TX_COMMAND_FAILED"; break;
        case RX_COMMAND_FAILED:           str = "RX_COMMAND_FAILED"; break;
        case ADDRESS_ACKNOWLDEGE_FAILED:  str = "ADDRESS_ACKNOWLDEGE_FAILED (no ACK from target)"; break;
        case DATA_ACKNOWLDEGE_FAILED:     str = "DATA_ACKNOWLDEGE_FAILED"; break;
        case MASTER_SLAVE_MODE_FAILED:    str = "MASTER_SLAVE_MODE_FAILED"; break;
        case ADDRESSING_MODE_FAILED:      str = "ADDRESSING_MODE_FAILED"; break;
        case INVALID_COMMAND:             str = "INVALID_COMMAND"; break;
        case INVALID_DATA_COUNT:          str = "INVALID_DATA_COUNT"; break;
        case TIME_OUT_ERROR:              str = "TIME_OUT_ERROR (no response from PRU)"; break;
        default:                          str = "unknown response"; break;
    }
    return str;
}

/* Wait until the PRU sets this instance's bit in the IRQ register.
 * The PRU also raises INTC system event SRSR1 bit 20 (PRU0) / 21 (PRU1) at
 * the same time (ICSS_I2C_INTC_PRU0_BIT_VAL / ICSS_I2C_INTC_PRU1_BIT_VAL).
 * This example does not use the interrupt, it polls the data RAM flag. */
static int32_t PruI2c_waitForPru(uint32_t timeoutUs)
{
    uint64_t start = ClockP_getTimeUsec();

    while ((HW_RD_REG16(gPruGlobalBase + PRU_I2C_IRQ_REG) & (1U << PRU_I2C_INST_ID)) == 0U)
    {
        if ((ClockP_getTimeUsec() - start) > timeoutUs)
        {
            return SystemP_TIMEOUT;
        }
    }
    return SystemP_SUCCESS;
}

/* Acknowledge the PRU response. The PRU waits for the bit to be cleared
 * before it accepts the next command. */
static void PruI2c_ackPru(void)
{
    uint16_t irq = HW_RD_REG16(gPruGlobalBase + PRU_I2C_IRQ_REG);

    irq &= (uint16_t)~(1U << PRU_I2C_INST_ID);
    HW_WR_REG16(gPruGlobalBase + PRU_I2C_IRQ_REG, irq);
}

/* Send a command to the PRU, wait for its response and acknowledge it. */
static int32_t PruI2c_sendCommand(uint16_t command, uint16_t *response)
{
    int32_t status;

    HW_WR_REG16(gPruDmemBase + PRU_I2C_COMMAND_REG, command);

    status = PruI2c_waitForPru(PRU_I2C_TIMEOUT_US);
    if (status == SystemP_SUCCESS)
    {
        *response = HW_RD_REG16(gPruDmemBase + PRU_I2C_RESPONSE_REG);
        PruI2c_ackPru();
    }
    else
    {
        *response = TIME_OUT_ERROR;
    }
    return status;
}

/* Write the I2C configuration into the PRU data RAM, load the firmware and
 * run the setup command. */
static int32_t PruI2c_init(void)
{
    const PRUICSS_HwAttrs *hwAttrs;
    uint16_t response = 0U;
    uint32_t reg;
    int32_t status;

    hwAttrs = PRUICSS_getAttrs(CONFIG_PRU_ICSS0);
    DebugP_assert(hwAttrs != NULL);
    gPruDmemBase = (PRU_I2C_PRU_CORE == 0U) ? hwAttrs->pru0DramBase : hwAttrs->pru1DramBase;
    gPruGlobalBase = hwAttrs->pru0DramBase;

    /* PRUICSS_loadFirmware() enables the PRU core right away, and the
     * firmware reads the frequency settings once while it boots. So stop the
     * core, clear the data RAM and write the whole configuration before the
     * firmware is loaded. */
    status = PRUICSS_disableCore(gPruIcss0Handle, PRU_I2C_PRU_CORE);
    if (status != SystemP_SUCCESS)
    {
        return status;
    }
    if (PRUICSS_initMemory(gPruIcss0Handle, PRUICSS_DATARAM(PRU_I2C_PRU_CORE)) == 0U)
    {
        return SystemP_FAILURE;
    }

    /* Bus frequency (u16) and IEP compare increment (u16), shared by both
     * cores. Clear only this instance's IRQ bit: the other core's instance
     * may be using the same register. */
    HW_WR_REG16(gPruGlobalBase + PRU_I2C_FREQ_REG, PRU_I2C_BUS_FREQ);
    HW_WR_REG16(gPruGlobalBase + PRU_I2C_IEP_INC_REG, PRU_I2C_IEP_INCREMENT);
    PruI2c_ackPru();

    /* Instance 0 configuration */
    /* TX and RX buffers are 256 bytes each */
    HW_WR_REG8(gPruDmemBase + PRU_I2C_TX_FIFO_REG, 0xFFU);
    HW_WR_REG8(gPruDmemBase + PRU_I2C_RX_FIFO_REG, 0xFFU);
    HW_WR_REG8(gPruDmemBase + PRU_I2C_SCL_PIN_REG, PRU_I2C_SCL_PIN);
    HW_WR_REG8(gPruDmemBase + PRU_I2C_SDA_PIN_REG, PRU_I2C_SDA_PIN);
    HW_WR_REG8(gPruDmemBase + PRU_I2C_INST_ID_REG, PRU_I2C_INST_ID);
    HW_WR_REG16(gPruDmemBase + PRU_I2C_SCL_TIMEOUT_REG, PRU_I2C_SCL_TIMEOUT_TICKS);
    /* Module enabled, master mode, 7-bit addressing, START and STOP around
     * every transfer, NACK the last byte of a read */
    reg = (1U << ICSS_I2C_MODULE_ENABLE_BIT) |
          (1U << ICSS_I2C_MASTER_SLAVE_MODE_BIT) |
          (1U << ICSS_I2C_START_BIT) |
          (1U << ICSS_I2C_STOP_BIT) |
          (1U << ICSS_I2C_RECIEVE_NACK_BIT);
    HW_WR_REG32(gPruDmemBase + PRU_I2C_CON_REG, reg);

    /* The bus is open-drain: the firmware keeps the R30 bits at 0 and drives
     * a line low by clearing its bit in ICSSM0_PRUx_GPIO_OUT_CTRL
     * (1 = output disabled = released). Start with both lines released, so
     * the PRU does not pull the bus low between reset and its setup.
     * SysConfig already sets these bits for pins configured with "rx". */
    reg = HW_RD_REG32(PRU_I2C_OUT_CTRL_REG);
    reg |= (1U << PRU_I2C_SCL_PIN) | (1U << PRU_I2C_SDA_PIN);
    HW_WR_REG32(PRU_I2C_OUT_CTRL_REG, reg);

    /* Load the firmware, this also starts the PRU core */
    status = PRUICSS_loadFirmware(gPruIcss0Handle, PRU_I2C_PRU_CORE,
                                  PRU_I2C_FIRMWARE, sizeof(PRU_I2C_FIRMWARE));
    if (status != SystemP_SUCCESS)
    {
        return status;
    }

    /* Latch the configuration inside the firmware */
    status = PruI2c_sendCommand(ICSS_I2C_SETUP_CMD, &response);
    if ((status != SystemP_SUCCESS) || (response != COMMAND_SUCCESS))
    {
        DebugP_log("PRU I2C setup failed, response 0x%04X (%s)\r\n",
                   response, PruI2c_responseToString(response));
        return SystemP_FAILURE;
    }
    return SystemP_SUCCESS;
}

/* Write tx[0..len-1] to the target. */
static int32_t PruI2c_write(uint8_t targetAddr, const uint8_t *tx, uint32_t len, uint16_t *response)
{
    uint32_t i;
    int32_t status;

    if ((len == 0U) || (len > PRU_I2C_MAX_COUNT))
    {
        *response = INVALID_DATA_COUNT;
        return SystemP_FAILURE;
    }

    HW_WR_REG16(gPruDmemBase + PRU_I2C_SA_REG, targetAddr);
    HW_WR_REG16(gPruDmemBase + PRU_I2C_COUNT_REG, len);
    for (i = 0U; i < len; i++)
    {
        HW_WR_REG8(gPruDmemBase + ICSS_I2C_INSTANCE0_TX_MEM + i, tx[i]);
    }

    status = PruI2c_sendCommand(ICSS_I2C_TX_CMD, response);
    if ((status == SystemP_SUCCESS) && (*response != COMMAND_SUCCESS))
    {
        status = SystemP_FAILURE;
    }
    return status;
}

/* Read len bytes from the target into rx. */
static int32_t PruI2c_read(uint8_t targetAddr, uint8_t *rx, uint32_t len, uint16_t *response)
{
    uint32_t i;
    int32_t status;

    if ((len == 0U) || (len > PRU_I2C_MAX_COUNT))
    {
        *response = INVALID_DATA_COUNT;
        return SystemP_FAILURE;
    }

    HW_WR_REG16(gPruDmemBase + PRU_I2C_SA_REG, targetAddr);
    HW_WR_REG16(gPruDmemBase + PRU_I2C_COUNT_REG, len);

    status = PruI2c_sendCommand(ICSS_I2C_RX_CMD, response);
    if (status == SystemP_SUCCESS)
    {
        if (*response == COMMAND_SUCCESS)
        {
            for (i = 0U; i < len; i++)
            {
                rx[i] = HW_RD_REG8(gPruDmemBase + ICSS_I2C_INSTANCE0_RX_MEM + i);
            }
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }
    return status;
}

/* ========================================================================== */
/*                                  Example                                   */
/* ========================================================================== */

void pru_i2c_example_main(void *args)
{
    uint8_t  txBuf[PRU_I2C_TEST_LEN + 1U];
    uint8_t  rxBuf[PRU_I2C_TEST_LEN];
    uint16_t response;
    uint32_t i;
    int32_t  status;
    int32_t  testStatus = SystemP_SUCCESS;

    Drivers_open(); // check return status

    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);

    gPruIcss0Handle = PRUICSS_open(CONFIG_PRU_ICSS0);
    DebugP_assert(gPruIcss0Handle != NULL);

    DebugP_log("PRU I2C master example started\r\n");
    DebugP_log("PRU core %u, SCL = PR0_PRU%u_GPIO%u, SDA = PR0_PRU%u_GPIO%u, target 0x%02X\r\n",
               PRU_I2C_PRU_CORE, PRU_I2C_PRU_CORE, PRU_I2C_SCL_PIN,
               PRU_I2C_PRU_CORE, PRU_I2C_SDA_PIN, PRU_I2C_TARGET_ADDR);

    status = PruI2c_init();
    if (status != SystemP_SUCCESS)
    {
        DebugP_log("FAIL: PRU I2C init (status %d)\r\n", status);
        testStatus = SystemP_FAILURE;
    }

    if (testStatus == SystemP_SUCCESS)
    {
        /* 1. write pointer + data bytes */
        txBuf[0] = PRU_I2C_TARGET_REG;
        for (i = 0U; i < PRU_I2C_TEST_LEN; i++)
        {
            txBuf[1U + i] = (uint8_t)(0xA5U ^ (i * 0x11U));
        }
        status = PruI2c_write(PRU_I2C_TARGET_ADDR, txBuf, PRU_I2C_TEST_LEN + 1U, &response);
        DebugP_log("Write %u bytes at pointer 0x%02X: response 0x%04X (%s)\r\n",
                   PRU_I2C_TEST_LEN, PRU_I2C_TARGET_REG, response, PruI2c_responseToString(response));
        if (status != SystemP_SUCCESS)
        {
            testStatus = SystemP_FAILURE;
        }
    }

    if (testStatus == SystemP_SUCCESS)
    {
        /* wait for the internal write cycle of the target */
        ClockP_usleep(PRU_I2C_WRITE_CYCLE_US);

        /* 2. set the pointer ... */
        status = PruI2c_write(PRU_I2C_TARGET_ADDR, txBuf, 1U, &response);
        DebugP_log("Write pointer 0x%02X: response 0x%04X (%s)\r\n",
                   PRU_I2C_TARGET_REG, response, PruI2c_responseToString(response));
        if (status != SystemP_SUCCESS)
        {
            testStatus = SystemP_FAILURE;
        }
    }

    if (testStatus == SystemP_SUCCESS)
    {
        /* 3. ... and read the data back */
        memset(rxBuf, 0, sizeof(rxBuf));
        status = PruI2c_read(PRU_I2C_TARGET_ADDR, rxBuf, PRU_I2C_TEST_LEN, &response);
        DebugP_log("Read %u bytes: response 0x%04X (%s)\r\n",
                   PRU_I2C_TEST_LEN, response, PruI2c_responseToString(response));
        if (status != SystemP_SUCCESS)
        {
            testStatus = SystemP_FAILURE;
        }
    }

    if (testStatus == SystemP_SUCCESS)
    {
        /* 4. compare */
        for (i = 0U; i < PRU_I2C_TEST_LEN; i++)
        {
            DebugP_log("  byte %u: wrote 0x%02X, read 0x%02X\r\n", i, txBuf[1U + i], rxBuf[i]);
            if (txBuf[1U + i] != rxBuf[i])
            {
                testStatus = SystemP_FAILURE;
            }
        }
        if (testStatus != SystemP_SUCCESS)
        {
            DebugP_log("FAIL: read back data does not match\r\n");
        }
    }

    if (testStatus == SystemP_SUCCESS)
    {
        DebugP_log("PASS: PRU I2C write/read back\r\n");
        DebugP_log("All tests have passed!!\r\n");
    }
    else
    {
        DebugP_log("Some tests have failed!!\r\n");
    }

    PRUICSS_disableCore(gPruIcss0Handle, PRU_I2C_PRU_CORE);
    PRUICSS_close(gPruIcss0Handle);
    Board_driversClose();
    Drivers_close();
}
