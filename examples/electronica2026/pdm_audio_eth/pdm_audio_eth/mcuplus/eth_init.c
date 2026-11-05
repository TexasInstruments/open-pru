/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

/*
 * eth_init.c - ICSSG1 Gigabit Ethernet bring-up for pdm_audio_eth.
 *
 * Ported from the HIL example's pru_eth_init.c (register write-up in its
 * pru_eth_init_register_doc.md), with these changes (docs/design.md 4,
 * plan Task 3):
 *  - the pads are set with the SDK's Pinmux_config(), which unlocks and
 *    relocks the PADCFG partitions itself (the HIL code wrote its kicks to
 *    0x43001008/0x43005008 and the KICK1 value into the KICK0 register);
 *  - HIL step 1 (C28) is done by the caller with PRUICSS_setConstantTblEntry;
 *  - HIL step 11 is dropped: it wrote 0x0B to the PRU0 CONTROL register,
 *    which enables the core before its firmware is loaded;
 *  - the PHY-address store to ICSSG1 shared RAM + 0x04 is dropped: that word
 *    is ETH1_VERSION here, and no firmware reads the table;
 *  - busy loops are replaced by ClockP_usleep().
 * Only port 1 (RGMII2, PHY address 3) carries traffic; port 0 is configured
 * as in the HIL example so both PHYs are in a known state.
 */

#include <stdint.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/pinmux.h>
#include <kernel/dpl/ClockP.h>
#include "eth_init.h"

/* ICSSG1 (global addresses) */
#define ICSSG1_BASE                 (0x30080000U)
#define ICSSG1_MII_RT_CFG           (ICSSG1_BASE + 0x00032000U)
#define ICSSG1_RXCFG0               (ICSSG1_MII_RT_CFG + 0x00U)
#define ICSSG1_RXCFG1               (ICSSG1_MII_RT_CFG + 0x04U)
#define ICSSG1_TXCFG0               (ICSSG1_MII_RT_CFG + 0x10U)
#define ICSSG1_TXCFG1               (ICSSG1_MII_RT_CFG + 0x14U)
#define ICSSG1_TX_IPG0              (ICSSG1_MII_RT_CFG + 0x30U)
#define ICSSG1_TX_IPG1              (ICSSG1_MII_RT_CFG + 0x34U)
#define ICSSG1_RX_FRMS0             (ICSSG1_MII_RT_CFG + 0x40U)
#define ICSSG1_RX_FRMS1             (ICSSG1_MII_RT_CFG + 0x44U)
#define ICSSG1_G_ICSS_G_CFG         (ICSSG1_BASE + 0x00033000U)
#define ICSSG1_G_RGMII_CFG          (ICSSG1_BASE + 0x00033004U)
#define ICSSG1_GPCFG1               (ICSSG1_BASE + 0x0002600CU)
#define ICSSG1_MDIO_CONTROL         (ICSSG1_BASE + 0x00032404U)
#define ICSSG1_MDIO_LINK            (ICSSG1_BASE + 0x0003240CU)
#define ICSSG1_MDIO_USERACCESS0     (ICSSG1_BASE + 0x00032480U)
#define ICSSG1_IEP_GLOBAL_CFG       (ICSSG1_BASE + 0x0002E000U)
#define ICSSG1_IEP_COUNT_LO         (ICSSG1_BASE + 0x0002E010U)   /* count_reg0 */
#define ICSSG1_IEP_COUNT_HI         (ICSSG1_BASE + 0x0002E014U)   /* count_reg1 */

/* Register values from the HIL example (pru_eth_init_register_doc.md) */
#define TXCFG_ALL_OFF               (0x00000800U)
#define G_ICSS_G_CFG_VAL            (0x0001022BU)   /* TX_L1/L2, RX_L2, RGMII both ports */
#define GPCFG1_MII_VAL              (0x08000003U)   /* PRU1 GPI = MII_RT, GP_MUX_SEL = MII */
#define TXCFG1_VAL                  (0x00000803U)   /* port 1: enable, auto preamble, PRU0 */
#define TXCFG0_VAL                  (0x00000903U)
#define RXCFG0_VAL                  (0x00000015U)
#define RXCFG1_VAL                  (0x0000021FU)   /* port 1: ... RX_EOF on R31 bit 20 */
#define RX_FRMS_MIN                 (0x06U)
#define TX_IPG_1G                   (0x00000018U)   /* 96 ns */
#define G_RGMII_CFG_VAL             (0x006600DDU)   /* both ports 1000 Mbit/s, FD, link */
#define IEP_CFG_333MHZ_NS           (0x00000331U)   /* enable, increment 3: counts ns */

/* MDIO / PHY */
#define MDIO_CTRL_ENABLE            (1U << 30)
#define MDIO_CLKDIV                 (0x000000FFU)
#define MDIO_UA_GO                  (1U << 31)
#define MDIO_UA_WRITE               (1U << 30)
#define MDIO_UA_ACK                 (1U << 29)
#define PHY_PORT0_ADDR              (15U)
#define PHY_PORT1_ADDR              (3U)
#define PHY_REG_BMSR                (0x01U)
#define PHY_REG_CTRL                (0x1FU)
#define PHY_CTRL_SW_RESET           (0x4000U)
#define PHY_EXT_REG_CTL             (0x0DU)
#define PHY_EXT_REG_ADAD            (0x0EU)
#define PHY_RGMII_CTRL_REG          (0x32U)
#define PHY_RGMII_TX_CLK_DELAY      (0xD2U)
#define MDIO_POLL_TRIES             (1000U)

/* Pads: RGMII2 (PRG1_PRU1_GPOx, mode 2) and MDIO0, values of HIL step 0 */
#define PAD_RGMII                   (PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE)
#define PAD_MDC                     (PIN_MODE(0) | PIN_PULL_DISABLE)
#define PAD_MDIO                    (PIN_MODE(0) | PIN_INPUT_ENABLE)

static const Pinmux_PerCfg_t gEthPinmux[] =
{
    { PIN_PRG1_MDIO0_MDC,  PAD_MDC },
    { PIN_PRG1_MDIO0_MDIO, PAD_MDIO },
    { PIN_PRG1_PRU1_GPO0,  PAD_RGMII },     /* RGMII2 RD0 */
    { PIN_PRG1_PRU1_GPO1,  PAD_RGMII },     /* RD1 */
    { PIN_PRG1_PRU1_GPO2,  PAD_RGMII },     /* RD2 */
    { PIN_PRG1_PRU1_GPO3,  PAD_RGMII },     /* RD3 */
    { PIN_PRG1_PRU1_GPO4,  PAD_RGMII },     /* RX_CTL */
    { PIN_PRG1_PRU1_GPO6,  PAD_RGMII },     /* RXC */
    { PIN_PRG1_PRU1_GPO11, PAD_RGMII },     /* TD0 */
    { PIN_PRG1_PRU1_GPO12, PAD_RGMII },     /* TD1 */
    { PIN_PRG1_PRU1_GPO13, PAD_RGMII },     /* TD2 */
    { PIN_PRG1_PRU1_GPO14, PAD_RGMII },     /* TD3 */
    { PIN_PRG1_PRU1_GPO15, PAD_RGMII },     /* TX_CTL */
    { PIN_PRG1_PRU1_GPO16, PAD_RGMII },     /* TXC */
    { PINMUX_END, 0U }
};

static int32_t mdio_wait(void)
{
    uint32_t tries;

    for (tries = 0U; tries < MDIO_POLL_TRIES; tries++)
    {
        if ((HW_RD_REG32(ICSSG1_MDIO_USERACCESS0) & MDIO_UA_GO) == 0U)
        {
            return 0;
        }
        ClockP_usleep(1U);
    }
    return -1;
}

static int32_t mdio_write(uint32_t phy, uint32_t reg, uint32_t data)
{
    if (mdio_wait() != 0)
    {
        return -1;
    }
    HW_WR_REG32(ICSSG1_MDIO_USERACCESS0,
                MDIO_UA_GO | MDIO_UA_WRITE | (reg << 21) | (phy << 16) | (data & 0xFFFFU));
    return mdio_wait();
}

static int32_t mdio_read(uint32_t phy, uint32_t reg)
{
    if (mdio_wait() != 0)
    {
        return -1;
    }
    HW_WR_REG32(ICSSG1_MDIO_USERACCESS0, MDIO_UA_GO | (reg << 21) | (phy << 16));
    if (mdio_wait() != 0)
    {
        return -1;
    }
    return ((HW_RD_REG32(ICSSG1_MDIO_USERACCESS0) & MDIO_UA_ACK) != 0U) ? 0 : -1;
}

static int32_t mdio_ext_write(uint32_t phy, uint32_t ext_reg, uint32_t data)
{
    int32_t status = mdio_write(phy, PHY_EXT_REG_CTL, 0x001FU);

    if (status == 0)
    {
        status = mdio_write(phy, PHY_EXT_REG_ADAD, ext_reg);
    }
    if (status == 0)
    {
        status = mdio_write(phy, PHY_EXT_REG_CTL, 0x401FU);
    }
    if (status == 0)
    {
        status = mdio_write(phy, PHY_EXT_REG_ADAD, data);
    }
    return status;
}

/* HIL step 13 for one PHY: alive, RGMII TX clock delay, software reset */
static int32_t phy_setup(uint32_t phy, int32_t errRead, int32_t errExt, int32_t errReset)
{
    if (mdio_read(phy, PHY_REG_BMSR) != 0)
    {
        return errRead;
    }
    if (mdio_ext_write(phy, PHY_RGMII_CTRL_REG, PHY_RGMII_TX_CLK_DELAY) != 0)
    {
        return errExt;
    }
    if (mdio_write(phy, PHY_REG_CTRL, PHY_CTRL_SW_RESET) != 0)
    {
        return errReset;
    }
    return ETH_INIT_OK;
}

int32_t eth_init(void)
{
    int32_t status;

    /* HIL step 0: pads */
    Pinmux_config(gEthPinmux, PINMUX_DOMAIN_ID_MAIN);

    /* Step 3: TX off while the FIFOs drain */
    HW_WR_REG32(ICSSG1_TXCFG0, TXCFG_ALL_OFF);
    ClockP_usleep(10U);

    /* Steps 4 to 9: global mode, PRU1 GPI in MII mode, TX/RX, IPG, RGMII */
    HW_WR_REG32(ICSSG1_G_ICSS_G_CFG, G_ICSS_G_CFG_VAL);
    HW_WR_REG32(ICSSG1_GPCFG1, GPCFG1_MII_VAL);
    HW_WR_REG32(ICSSG1_TXCFG1, TXCFG1_VAL);
    HW_WR_REG32(ICSSG1_TXCFG0, TXCFG0_VAL);
    HW_WR_REG32(ICSSG1_RXCFG0, RXCFG0_VAL);
    HW_WR_REG32(ICSSG1_RXCFG1, RXCFG1_VAL);
    HW_WR_REG8(ICSSG1_RX_FRMS0, RX_FRMS_MIN);
    HW_WR_REG8(ICSSG1_RX_FRMS1, RX_FRMS_MIN);
    HW_WR_REG32(ICSSG1_TX_IPG0, TX_IPG_1G);
    HW_WR_REG32(ICSSG1_TX_IPG1, TX_IPG_1G);
    HW_WR_REG32(ICSSG1_G_RGMII_CFG, G_RGMII_CFG_VAL);

    /* Step 10: IEP0 counts ns from 0 */
    HW_WR_REG32(ICSSG1_IEP_GLOBAL_CFG, HW_RD_REG32(ICSSG1_IEP_GLOBAL_CFG) & ~1U);
    HW_WR_REG32(ICSSG1_IEP_COUNT_LO, 0xFFFFFFFFU);  /* write 1 to clear */
    HW_WR_REG32(ICSSG1_IEP_COUNT_HI, 0xFFFFFFFFU);
    HW_WR_REG32(ICSSG1_IEP_GLOBAL_CFG, IEP_CFG_333MHZ_NS);

    /* Step 12: MDIO on */
    HW_WR_REG32(ICSSG1_MDIO_CONTROL, MDIO_CTRL_ENABLE | MDIO_CLKDIV);
    ClockP_usleep(1000U);

    /* Step 13: both PHYs */
    status = phy_setup(PHY_PORT0_ADDR, ETH_INIT_ERR_PHY1_READ, ETH_INIT_ERR_PHY1_EXT,
                       ETH_INIT_ERR_PHY1_RESET);
    if (status == ETH_INIT_OK)
    {
        status = phy_setup(PHY_PORT1_ADDR, ETH_INIT_ERR_PHY2_READ, ETH_INIT_ERR_PHY2_EXT,
                           ETH_INIT_ERR_PHY2_RESET);
    }
    return status;
}

int32_t eth_wait_for_link(uint32_t timeout_ms)
{
    uint32_t ms;

    for (ms = 0U; ms < timeout_ms; ms++)
    {
        if ((HW_RD_REG32(ICSSG1_MDIO_LINK) & (1U << PHY_PORT1_ADDR)) != 0U)
        {
            return 0;
        }
        ClockP_usleep(1000U);
    }
    return -1;
}
