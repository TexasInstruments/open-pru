// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use std::process::ExitCode;
use std::time::Duration;

use clap::{Args, Parser, Subcommand};

use pdm_eth::emulator::{EmuConfig, Emulator};
use pdm_eth::ethtest::{self, TestConfig};
use pdm_eth::link::{link_info, AfPacketLink, Link};

/// PC application of the pdm_audio_eth demo (LP-AM243 PDM audio over Gigabit Ethernet).
#[derive(Parser)]
#[command(name = "pdm-eth", version)]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

/// Where the board is.
#[derive(Args, Clone)]
struct Target {
    /// Network interface with the direct cable to the LP-AM243
    #[arg(long, default_value = "enp129s0")]
    iface: String,
    /// Use the in-process board emulator instead of the interface
    #[arg(long)]
    emulator: bool,
}

#[derive(Subcommand)]
enum Cmd {
    /// Ethernet test: link, ping-pong round trip, throughput both ways
    Test {
        #[command(flatten)]
        target: Target,
        /// Number of pings
        #[arg(long, default_value_t = 10_000)]
        pings: u32,
        /// Seconds per throughput direction
        #[arg(long, default_value_t = 3.0)]
        seconds: f64,
    },
}

fn open(t: &Target) -> Result<(Box<dyn Link>, (u32, bool, bool)), String> {
    if t.emulator {
        return Ok((Box::new(Emulator::new(EmuConfig::default())), (1000, true, true)));
    }
    let info = link_info(&t.iface).map_err(|e| format!("{}: {e}", t.iface))?;
    let link = AfPacketLink::open(&t.iface).map_err(|e| e.to_string())?;
    Ok((Box::new(link), info))
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::Test { target, pings, seconds } => {
            let (mut link, info) = match open(&target) {
                Ok(x) => x,
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            let cfg = TestConfig { pings, ping_timeout: Duration::from_millis(100), seconds };
            let s = match ethtest::run(link.as_mut(), &cfg, info) {
                Ok(s) => s,
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            let mut all = true;
            for (name, ok, text) in s.verdicts() {
                println!("{} {name}: {text}", if ok { "PASS" } else { "FAIL" });
                all &= ok;
            }
            if all { ExitCode::SUCCESS } else { ExitCode::FAILURE }
        }
    }
}
