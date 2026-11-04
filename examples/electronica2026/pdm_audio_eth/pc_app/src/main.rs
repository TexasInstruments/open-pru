// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use std::path::PathBuf;
use std::process::ExitCode;
use std::time::Duration;

use clap::{Args, Parser, Subcommand};

use pdm_eth::emulator::{EmuConfig, Emulator};
use pdm_eth::ethtest::{self, TestConfig};
use pdm_eth::link::{link_info, now_ns, AfPacketLink, Link};
use pdm_eth::pacing::Pacer;
use pdm_eth::proto::{decode, encode, Body, FrameType, Header, BOARD_MAC, VERSION};
use pdm_eth::wavin::{load_channel, InputSet};
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

/// One WAV file per channel; a channel without a file is silent.
#[derive(Args, Clone, Default)]
struct Inputs {
    #[arg(long)]
    ch0: Option<PathBuf>,
    #[arg(long)]
    ch1: Option<PathBuf>,
    #[arg(long)]
    ch2: Option<PathBuf>,
    #[arg(long)]
    ch3: Option<PathBuf>,
    #[arg(long)]
    ch4: Option<PathBuf>,
    #[arg(long)]
    ch5: Option<PathBuf>,
    #[arg(long)]
    ch6: Option<PathBuf>,
    #[arg(long)]
    ch7: Option<PathBuf>,
}

impl Inputs {
    fn load(&self) -> Result<Option<InputSet>, String> {
        let files = [&self.ch0, &self.ch1, &self.ch2, &self.ch3, &self.ch4, &self.ch5, &self.ch6, &self.ch7];
        if files.iter().all(|f| f.is_none()) {
            return Ok(None);
        }
        let mut set = InputSet::default();
        for (c, f) in files.iter().enumerate() {
            if let Some(p) = f {
                set.ch[c] = load_channel(p).map_err(|e| e.to_string())?;
            }
        }
        Ok(Some(set))
    }
}

/// Send the input block of stream frames `pos..pos + 64` with board index `first`.
pub fn send_block(link: &mut dyn Link, set: &InputSet, pos: usize, first: u32, seq: u32) -> std::io::Result<[[i16; 8]; 64]> {
    let mut pcm = Box::new([[0i16; 8]; 64]);
    for (k, row) in pcm.iter_mut().enumerate() {
        *row = set.frame(pos + k);
    }
    let h = Header { version: VERSION, ftype: FrameType::AudioIn, length: 0, seq, ts: now_ns() };
    let mac = link.mac();
    let block = *pcm;
    link.send(&encode(BOARD_MAC, mac, &h, &Body::AudioIn { first_index: first, pcm }, 0))?;
    Ok(block)
}

fn stream(link: &mut dyn Link, set: &InputSet, capture: Option<&std::path::Path>, lead: u32) -> std::io::Result<()> {
    ethtest::hello(link)?;
    let mut out = OutStream::new();
    let mut pacer = Pacer::new(lead, 16384);
    let mut caps = match capture {
        Some(d) => Some((WavCapture::create_bits(d, "in", 16)?, WavCapture::create(d, "out")?)),
        None => None,
    };
    let (mut pos, mut seq, mut done_at) = (0usize, 0u32, None::<u64>);
    let total = set.frames();
    let tail_ns = (lead as u64 + 4000) * 1_000_000_000 / 16_000; // drain the lead and the pipeline
    loop {
        let now = now_ns();
        if done_at.is_some_and(|t| now > t + tail_ns) {
            break;
        }
        if let Some(f) = link.recv(Duration::from_millis(1))?
            && let Ok((h, Body::AudioOut { status, pcm }, _)) = decode(&f.data) {
                out.push(&status, &pcm, h.ts, f.t_ns);
                pacer.on_status(&status, f.t_ns);
                let v = out.take_new();
                if let Some((_, o)) = &mut caps {
                    o.write(&v)?;
                }
            }
        while done_at.is_none() {
            let Some(first) = pacer.next_due(now_ns()) else { break };
            let block = send_block(link, set, pos, first, seq)?;
            if let Some((i, _)) = &mut caps {
                let v: Vec<(u64, [i32; 8])> = block
                    .iter()
                    .enumerate()
                    .map(|(k, f)| ((pos + k) as u64, f.map(|x| x as i32)))
                    .collect();
                i.write(&v)?;
            }
            seq += 1;
            pos += 64;
            if pos >= total {
                done_at = Some(now_ns());
                pacer.finish();
            }
        }
    }
    if let Some((i, o)) = caps {
        i.finish()?;
        o.finish()?;
    }
    let st = out.last_status().copied().unwrap_or_default();
    println!("sent {} blocks ({} frames of {total}); underruns {} overwrite_blocked {} restarts {}",
             seq, pos, pacer.underruns, pacer.overwrite_blocked, pacer.restarts);
    println!("output lost {} duplicates {} resyncs {}; board: rx_audio {} last_in_end {} underrun {} sd_ovf {} tx_late {}",
             out.lost_frames, out.duplicates, out.resyncs, st.rx_audio, st.last_in_end, st.underrun_count,
             st.sd_ovf_count, st.tx_late);
    Ok(())
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
    /// Stream WAV files (one per channel) into the board, optionally capture
    Stream {
        #[command(flatten)]
        target: Target,
        #[command(flatten)]
        inputs: Inputs,
        /// Capture input (in_ch*.wav, 16-bit) and output (out_ch*.wav) here
        #[arg(long)]
        capture: Option<PathBuf>,
        /// Frames the PC stays ahead of RTU1 (8000 = 0.5 s)
        #[arg(long, default_value_t = 8000)]
        lead: u32,
    },
    /// Live window: spectrograms and numbers (layout C)
    Gui {
        #[command(flatten)]
        target: Target,
        /// Input files, looped; without any the window shows the output only
        #[command(flatten)]
        inputs: Inputs,
        /// Frames the PC stays ahead of RTU1 (8000 = 0.5 s)
        #[arg(long, default_value_t = 8000)]
        lead: u32,
        /// Export aligned input/output blocks to /dev/shm/NAME (processing seam)
        #[arg(long)]
        export_shm: Option<String>,
        /// Save a PNG of the window after 5 s and exit (documentation;
        /// implies --no-vsync; under Wayland run it with WAYLAND_DISPLAY unset)
        #[arg(long)]
        screenshot: Option<PathBuf>,
        /// Draw without waiting for vsync (a hidden window is otherwise
        /// throttled by the compositor)
        #[arg(long)]
        no_vsync: bool,
    },
}

fn gui(target: &Target, inputs: &Inputs, lead: u32, export_shm: Option<String>, screenshot: Option<PathBuf>,
       no_vsync: bool) -> Result<(), String> {
    let set = inputs.load()?;
    let export = match export_shm {
        Some(n) => Some(pdm_eth::seam::ShmExport::create(&n, 256).map_err(|e| format!("/dev/shm/{n}: {e}"))?),
        None => None,
    };
    let no_vsync = no_vsync || screenshot.is_some();
    let (link, _) = open(target)?;
    let source = if target.emulator { "emulator".to_string() } else { target.iface.clone() };
    let (tx, rx) = std::sync::mpsc::channel();
    let options = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default()
            .with_title("pdm_audio_eth")
            .with_inner_size([1400.0, 820.0]),
        wgpu_options: {
            let mut w = eframe::egui_wgpu::WgpuConfiguration::default();
            if no_vsync {
                w.surface.present_mode = eframe::wgpu::PresentMode::AutoNoVsync;
            }
            w
        },
        ..Default::default()
    };
    eframe::run_native(
        "pdm_audio_eth",
        options,
        Box::new(move |cc| {
            let sel = std::sync::Arc::new(std::sync::atomic::AtomicUsize::new(0));
            let input = set.map(|set| pdm_eth::ui::NetInput { set, lead, sel: sel.clone(), export });
            let ctx = cc.egui_ctx.clone();
            let repaint: std::sync::Arc<dyn Fn() + Send + Sync> = std::sync::Arc::new(move || ctx.request_repaint());
            std::thread::spawn(move || pdm_eth::ui::run_net(link, input, tx, repaint));
            let app = pdm_eth::ui::App::new(rx, source).with_selection(sel);
            Ok(Box::new(match screenshot {
                Some(p) => app.with_screenshot(p, Duration::from_secs(5)),
                None => app,
            }))
        }),
    )
    .map_err(|e| e.to_string())
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

/// The link and its (Mbit/s, full duplex, carrier).
type Opened = (Box<dyn Link>, (u32, bool, bool));

fn open(t: &Target) -> Result<Opened, String> {
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
        Cmd::Gui { target, inputs, lead, export_shm, screenshot, no_vsync } => match gui(&target, &inputs, lead, export_shm, screenshot, no_vsync) {
            Ok(()) => ExitCode::SUCCESS,
            Err(e) => {
                eprintln!("error: {e}");
                ExitCode::from(2)
            }
        },
        Cmd::Stream { target, inputs, capture, lead } => {
            let set = match inputs.load() {
                Ok(Some(s)) => s,
                Ok(None) => {
                    eprintln!("error: give at least one --chN file");
                    return ExitCode::from(2);
                }
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            let (mut link, _) = match open(&target) {
                Ok(x) => x,
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            match stream(link.as_mut(), &set, capture.as_deref(), lead) {
                Ok(()) => ExitCode::SUCCESS,
                Err(e) => {
                    eprintln!("error: {e}");
                    ExitCode::from(2)
                }
            }
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
