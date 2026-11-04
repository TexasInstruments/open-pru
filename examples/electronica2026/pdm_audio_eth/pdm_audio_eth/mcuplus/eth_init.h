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

/**
 * \brief Wait until the MDIO link register reports the port-1 PHY up.
 * \param timeout_ms  maximum wait
 * \return 0 when the link is up, -1 on timeout
 */
int32_t eth_wait_for_link(uint32_t timeout_ms);

#endif /* ETH_INIT_H_ */
