// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use clap::Parser;
use eframe::egui;
use std::sync::{Arc, Mutex};
use std::time::Instant;
use viz::cli::Args;
use viz::ui::VizApp;
use viz::{Conn, Tracker, poller};

const TITLE: &str = "I2C LED visualizer";

fn main() -> eframe::Result {
    let args = Args::parse();
    if args.no_gui {
        run_headless(&args);
    }
    let shared = Arc::new(Mutex::new(Tracker::new().snapshot()));
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_title(TITLE).with_inner_size([640.0, 360.0]),
        ..Default::default()
    };
    eframe::run_native(
        TITLE,
        options,
        Box::new(move |cc| {
            let ctx = cc.egui_ctx.clone();
            poller::spawn(&args, shared.clone(), Box::new(move || ctx.request_repaint()));
            Ok(Box::new(VizApp::new(shared)))
        }),
    )
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
