// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use std::path::PathBuf;
use std::process::ExitCode;
use std::time::Duration;

use clap::{Args, Parser, Subcommand};

use pdm_eth::emulator::{EmuConfig, Emulator};
use pdm_eth::ethtest::{self, TestConfig};
use pdm_eth::link::{link_info, now_ns, AfPacketLink, Link};
use pdm_eth::proto::{decode, Body};
use pdm_eth::rx::{OutStream, WavCapture};

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
    /// Capture the board output to out_ch0..7.wav (16 kHz, 24-bit)
    Capture {
        #[command(flatten)]
        target: Target,
        /// Seconds to capture
        #[arg(long, default_value_t = 10.0)]
        seconds: f64,
        /// Output directory
        #[arg(long)]
        out: PathBuf,
    },
}

fn capture(link: &mut dyn Link, seconds: f64, out: &std::path::Path) -> std::io::Result<()> {
    ethtest::hello(link)?;
    let mut s = OutStream::new();
    let mut wav = WavCapture::create(out, "out")?;
    let (mut written, end) = (0u64, now_ns() + (seconds * 1e9) as u64);
    while now_ns() < end {
        let Some(f) = link.recv(Duration::from_millis(100))? else { continue };
        if let Ok((h, Body::AudioOut { status, pcm }, _)) = decode(&f.data) {
            s.push(&status, &pcm, h.ts, f.t_ns);
            let v = s.take_new();
            written += v.len() as u64;
            wav.write(&v)?;
        }
    }
    wav.finish()?;
    let st = s.last_status().copied().unwrap_or_default();
    println!("{written} frames per channel written to {}", out.display());
    println!("lost {} duplicates {} resyncs {}; board: underrun {} sd_ovf {} tx_late {} rx_foreign {} rx_bad {}",
             s.lost_frames, s.duplicates, s.resyncs, st.underrun_count, st.sd_ovf_count, st.tx_late,
             st.rx_foreign, st.rx_bad);
    if let Some(ppm) = s.board_clock_ppm() {
        println!("board clock {ppm:+.1} ppm against the PC");
    }
    Ok(())
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
        Cmd::Capture { target, seconds, out } => {
            let (mut link, _) = match open(&target) {
                Ok(x) => x,
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            match capture(link.as_mut(), seconds, &out) {
                Ok(()) => ExitCode::SUCCESS,
                Err(e) => {
                    eprintln!("error: {e}");
                    ExitCode::from(2)
                }
            }
        }
    }
}
