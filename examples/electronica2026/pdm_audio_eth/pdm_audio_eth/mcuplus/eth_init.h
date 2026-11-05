/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

/*
 * eth_init.h - ICSSG1 Gigabit Ethernet bring-up for pdm_audio_eth: pads,
 * MII_RT, RGMII, IEP, MDIO and the DP83869 PHYs. Ported from the user's HIL
 * example (pru_eth_init.c, docs/design.md section 4). The ICSSG1 PRU cores
 * must be halted while eth_init() runs.
 */

#ifndef ETH_INIT_H_
#define ETH_INIT_H_

#include <stdint.h>

/** \brief Error codes of eth_init() (same numbers as the HIL example) */
#define ETH_INIT_OK                 (0)
#define ETH_INIT_ERR_PHY1_READ      (-1)
#define ETH_INIT_ERR_PHY1_EXT       (-2)
#define ETH_INIT_ERR_PHY1_RESET     (-3)
#define ETH_INIT_ERR_PHY2_READ      (-4)
#define ETH_INIT_ERR_PHY2_EXT       (-5)
#define ETH_INIT_ERR_PHY2_RESET     (-6)

/**
 * \brief Configure ICSSG1 for raw Ethernet on port 1 (RGMII2, PHY address 3)
 *        at 1000 Mbit/s full duplex: ICSSG1 PRU0 transmits through TX_L2,
 *        PRU1 receives through RX_L2 with RX_EOF on R31 bit 20, IEP0 counts
 *        in ns (333 MHz, increment 3).
 * \return ETH_INIT_OK or an ETH_INIT_ERR_* code
 */
int32_t eth_init(void);

/** \brief Port-1 link state from eth_wait_for_link() */
typedef struct
{
    uint32_t wait_ms;       /**< time until the link was up (or the timeout) */
    uint32_t physts;        /**< last DP83869 PHYSTS (register 0x11) read */
    uint32_t speed;         /**< PHYSTS speed: 0 = 10, 1 = 100, 2 = 1000 Mbit/s */
    uint32_t full;          /**< 1 = full duplex */
    uint32_t mdio_link;     /**< MDIO_LINK register (poll state machine), for debug */
    uint32_t mdio_alive;    /**< MDIO_ALIVE register, for debug */
} EthLinkInfo;

/**
 * \brief Wait until the port-1 PHY (address 3) reports link with speed and
 *        duplex resolved, polling its PHYSTS register every 10 ms.
 * \param timeout_ms  maximum wait
 * \param info        filled in on return (also on timeout)
 * \return 0 when the link is up, -1 on timeout
 */
int32_t eth_wait_for_link(uint32_t timeout_ms, EthLinkInfo *info);

#endif /* ETH_INIT_H_ */
