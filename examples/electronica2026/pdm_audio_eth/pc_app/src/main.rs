// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use clap::Parser;

/// PC application of the pdm_audio_eth demo (LP-AM243 PDM audio over Gigabit Ethernet).
#[derive(Parser)]
#[command(name = "pdm-eth", version)]
struct Cli {}

fn main() {
    let _cli = Cli::parse();
}
