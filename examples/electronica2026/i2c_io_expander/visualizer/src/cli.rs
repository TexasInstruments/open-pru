// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Command line.

use clap::Parser;

/// Shows the TCA9538's 8 LEDs, driven by PicoScope 7's I2C decode
/// (electronica 2026 i2c_io demo).
#[derive(Parser, Debug, Clone)]
#[command(version)]
pub struct Args {
    /// PicoScope 7 MCP server URL.
    #[arg(long, default_value = "http://127.0.0.1:18123/mcp")]
    pub url: String,
    /// Poll interval in ms, start to start.
    #[arg(long, default_value_t = 50)]
    pub interval_ms: u64,
    /// Print reply times, changes and skipped steps once per second.
    #[arg(long)]
    pub stats: bool,
    /// No window: print each LED change to the terminal (for measurements).
    #[arg(long)]
    pub no_gui: bool,
}
