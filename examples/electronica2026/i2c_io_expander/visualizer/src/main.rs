// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use clap::Parser;
use std::sync::Mutex;
use std::time::Instant;
use viz::cli::Args;
use viz::{Conn, Tracker, poller};

fn main() {
    let args = Args::parse();
    if args.no_gui {
        run_headless(&args);
    }
}

/// `--no-gui`: print every change with the time since start.
fn run_headless(args: &Args) -> ! {
    let t0 = Instant::now();
    let shared = Mutex::new(Tracker::new().snapshot());
    poller::run(args, &shared, &|s| {
        let t = t0.elapsed().as_secs_f64();
        match (s.conn, s.value) {
            (Conn::Live, Some(v)) => println!("{t:9.3}s data 0x{v:02x}"),
            _ => println!("{t:9.3}s {}", s.status_text()),
        }
    })
}
