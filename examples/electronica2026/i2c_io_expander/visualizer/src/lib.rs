// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! I2C LED visualizer: PicoScope 7 I2C decode -> TCA9538 LED view.

pub mod decode;
#[cfg(test)]
mod fake;
pub mod cli;
pub mod mcp;
pub mod poller;
pub mod setup;
pub mod stats;
pub mod tracker;

pub use decode::{Pick, parse_address_key, parse_frame_key, pick, successor};
pub use mcp::{McpClient, McpError, ToolCaller};
pub use setup::ensure_decoder;
pub use tracker::{Conn, PollResult, Snapshot, Tracker};
