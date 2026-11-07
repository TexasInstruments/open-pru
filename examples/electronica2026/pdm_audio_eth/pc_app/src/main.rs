// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

use std::path::PathBuf;
use std::process::ExitCode;
use std::time::Duration;

use clap::{Args, Parser, Subcommand};

use pdm_eth::emulator::{EmuConfig, Emulator};
use pdm_eth::ethtest::{self, TestConfig};
use pdm_eth::link::{link_info, AfPacketLink, Link};
use pdm_eth::wavin::{load_channel, load_set_on, InputSet};
use pdm_eth::rx::WavCapture;

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
    /// Load the --chN files as one capture set: one gain for all (the set's
    /// loudest sample maps to 0.5 FS), trimmed to the shortest, so the level
    /// differences between the microphones are kept. Without it every file
    /// is scaled to 0.5 FS on its own.
    #[arg(long)]
    set: bool,
}

impl Inputs {
    /// File name per channel, for the GUI labels.
    fn names(&self) -> [Option<String>; 8] {
        let files = [&self.ch0, &self.ch1, &self.ch2, &self.ch3, &self.ch4, &self.ch5, &self.ch6, &self.ch7];
        std::array::from_fn(|c| files[c].as_ref().and_then(|p| p.file_name()).map(|n| n.to_string_lossy().into_owned()))
    }

    fn load(&self) -> Result<Option<InputSet>, String> {
        let files = [&self.ch0, &self.ch1, &self.ch2, &self.ch3, &self.ch4, &self.ch5, &self.ch6, &self.ch7];
        if files.iter().all(|f| f.is_none()) {
            return Ok(None);
        }
        if self.set {
            let on: Vec<(usize, PathBuf)> =
                files.iter().enumerate().filter_map(|(c, f)| f.as_ref().map(|p| (c, p.to_path_buf()))).collect();
            return load_set_on(&on).map(Some).map_err(|e| e.to_string());
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
        /// Export aligned input/output blocks to /dev/shm/NAME (processing
        /// seam; a ring of STREAM_SHM_BLOCKS blocks = 16 s)
        #[arg(long)]
        export_shm: Option<String>,
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
        /// Seconds after the start for --screenshot
        #[arg(long, default_value_t = 5.0)]
        screenshot_after: f64,
        /// Draw without waiting for vsync (a hidden window is otherwise
        /// throttled by the compositor)
        #[arg(long)]
        no_vsync: bool,
        /// Booth playlist: every subfolder with mic1..8.wav is a scene,
        /// played in name order, each followed by 1 s of silence, looped
        #[arg(long, conflicts_with_all = ["ch0", "ch1", "ch2", "ch3", "ch4", "ch5", "ch6", "ch7", "set"])]
        playlist: Option<PathBuf>,
        /// Per-talker text lanes: run this audio_pipe binary as `BIN live`
        /// on the board output (needs --playlist; exports /dev/shm/pdm-eth-<pid>
        /// unless --export-shm names the ring)
        #[arg(long, value_name = "BIN")]
        transcribe: Option<PathBuf>,
        /// Append every text line and pass start (JSON lines) to this file
        /// (input of booth_curate.py)
        #[arg(long, value_name = "FILE")]
        transcript_log: Option<PathBuf>,
    },
}

/// The --transcribe settings of the window.
struct Transcribe {
    bin: Option<PathBuf>,
    log: Option<PathBuf>,
    screenshot_after: f64,
}

fn gui(target: &Target, inputs: &Inputs, lead: u32, export_shm: Option<String>, screenshot: Option<PathBuf>,
       no_vsync: bool, playlist: Option<PathBuf>, tr: Transcribe) -> Result<(), String> {
    if tr.bin.is_some() && playlist.is_none() {
        return Err("--transcribe needs --playlist".into());
    }
    let mut names = inputs.names();
    let shot_after = tr.screenshot_after.max(0.0);
    let mut scales = Vec::new();
    let (set, marks, scenes) = match &playlist {
        Some(dir) => {
            let pl = pdm_eth::playlist::load_playlist(dir).map_err(|e| e.to_string())?;
            eprintln!("playlist {}: {} scenes, loop {:.1} s", dir.display(), pl.passes.len(), pl.total as f64 / 16000.0);
            // the folder name only: a long label pushes the side panel out of the window
            let short = dir.canonicalize().ok().and_then(|d| d.file_name().map(|n| n.to_string_lossy().into_owned()))
                .unwrap_or_else(|| dir.display().to_string());
            names = std::array::from_fn(|_| Some(format!("playlist {short}")));
            scales = pl.passes.iter().map(|p| p.scale).collect();
            (pl.set, pl.passes.iter().map(|p| p.offset).collect(), pl.passes.iter().map(|p| p.scene.clone()).collect())
        }
        // stream from the start; channels without a --chN file play their
        // booth clip (design 7.2a)
        None => (InputSet::gui_start(inputs.load()?), Vec::new(), Vec::new()),
    };
    let set = Some(set);
    // the text needs a ring audio_pipe can lag behind by a GPU stall: 1024 blocks (4 s)
    let (export_shm, blocks) = match (&tr.bin, export_shm) {
        (Some(_), n) => (Some(n.unwrap_or_else(|| format!("pdm-eth-{}", std::process::id()))), 1024),
        (None, n) => (n, 256),
    };
    let export = match &export_shm {
        Some(n) => Some(pdm_eth::seam::ShmExport::create(n, blocks).map_err(|e| format!("/dev/shm/{n}: {e}"))?),
        None => None,
    };
    let text = match &tr.bin {
        Some(bin) => {
            let argv = vec![bin.display().to_string(), "live".into(), "--shm".into(), export_shm.clone().unwrap()];
            let log = match &tr.log {
                Some(p) => Some(std::fs::OpenOptions::new().create(true).append(true).open(p)
                    .map_err(|e| format!("{}: {e}", p.display()))?),
                None => None,
            };
            Some((pdm_eth::transcribe::TextPipe::new(argv), log))
        }
        None => None,
    };
    let no_vsync = no_vsync || screenshot.is_some();
    let (link, _) = open(target, false)?;
    let pin_iface = (!target.emulator).then(|| target.iface.clone());
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
            let input = set.map(|set| pdm_eth::ui::NetInput { set, lead, sel: sel.clone(), export, marks });
            let (cmd_tx, cmd_rx) = std::sync::mpsc::channel();
            let ctl = pdm_eth::ui::NetCtl { cmds: cmd_rx, lead, sel: sel.clone() };
            let ctx = cc.egui_ctx.clone();
            let repaint: std::sync::Arc<dyn Fn() + Send + Sync> = std::sync::Arc::new(move || ctx.request_repaint());
            std::thread::spawn(move || {
                if let Some(i) = &pin_iface {
                    pdm_eth::link::pin_for_nic(i);
                }
                pdm_eth::ui::run_net(link, input, ctl, tx, repaint)
            });
            let mut app = pdm_eth::ui::App::new(rx, source).with_selection(sel).with_commands(cmd_tx).with_playlist(scenes);
            if let Some((tp, log)) = text {
                app = app.with_transcription(tp, scales, log);
            }
            let app = app
                .with_sources(names.map(|n| n.map_or(pdm_eth::sources::Source::Booth, pdm_eth::sources::Source::File)));
            Ok(Box::new(match screenshot {
                Some(p) => app.with_screenshot(p, Duration::from_secs_f64(shot_after)),
                None => app,
            }))
        }),
    )
    .map_err(|e| e.to_string())
}

/// The link and its (Mbit/s, full duplex, carrier).
type Opened = (Box<dyn Link>, pdm_eth::link::LinkInfo);

/// `pin`: pin this thread for NIC work (the GUI pins its network thread).
fn open(t: &Target, pin: bool) -> Result<Opened, String> {
    if t.emulator {
        return Ok((Box::new(Emulator::new(EmuConfig::default())), (1000, Some(true), true)));
    }
    let info = link_info(&t.iface).map_err(|e| format!("{}: {e}", t.iface))?;
    let link = AfPacketLink::open(&t.iface).map_err(|e| e.to_string())?;
    if pin {
        pdm_eth::link::pin_for_nic(&t.iface);
    }
    Ok((Box::new(link), info))
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::Test { target, pings, seconds } => {
            let (mut link, info) = match open(&target, true) {
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
        Cmd::Gui { target, inputs, lead, export_shm, screenshot, screenshot_after, no_vsync, playlist, transcribe, transcript_log } =>
            match gui(&target, &inputs, lead, export_shm, screenshot, no_vsync, playlist,
                      Transcribe { bin: transcribe, log: transcript_log, screenshot_after }) {
            Ok(()) => ExitCode::SUCCESS,
            Err(e) => {
                eprintln!("error: {e}");
                ExitCode::from(2)
            }
        },
        Cmd::Stream { target, inputs, capture, lead, export_shm } => {
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
            let (mut link, _) = match open(&target, true) {
                Ok(x) => x,
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            let mut export = match &export_shm {
                Some(n) => match pdm_eth::seam::ShmExport::create(n, pdm_eth::session::STREAM_SHM_BLOCKS) {
                    Ok(x) => Some(x),
                    Err(e) => {
                        eprintln!("error: /dev/shm/{n}: {e}");
                        return ExitCode::from(2);
                    }
                },
                None => None,
            };
            match pdm_eth::session::stream(link.as_mut(), &set, capture.as_deref(), lead, export.as_mut()) {
                Ok(r) => {
                    println!("sent {} blocks ({} frames of {}); underruns {} overwrite_blocked {} restarts {}",
                             r.sent_blocks, r.sent_frames, set.frames(), r.underruns, r.overwrite_blocked, r.restarts);
                    let st = r.status;
                    println!("output lost {} duplicates {} resyncs {}; board: rx_audio {} last_in_end {} underrun {} sd_ovf {} tx_late {}",
                             r.lost, r.duplicates, r.resyncs, st.rx_audio, st.last_in_end, st.underrun_count,
                             st.sd_ovf_count, st.tx_late);
                    match r.lag {
                        Some(l) => println!("lag {l} frames (output index k carries input index k + lag), board audio latency {}",
                                            r.latency_ms.map_or("-".into(), |v| format!("{v:.2} ms"))),
                        None => println!("lag not found (input and output did not correlate)"),
                    }
                    if let Some(n) = &export_shm {
                        println!("exported {} aligned blocks to /dev/shm/{n}, first board index {}", r.exported_blocks,
                                 r.first_index.map_or("-".into(), |v| v.to_string()));
                    }
                    ExitCode::SUCCESS
                }
                Err(e) => {
                    eprintln!("error: {e}");
                    ExitCode::from(2)
                }
            }
        }
        Cmd::Capture { target, seconds, out } => {
            let (mut link, _) = match open(&target, true) {
                Ok(x) => x,
                Err(e) => {
                    eprintln!("error: {e}");
                    return ExitCode::from(2);
                }
            };
            let mut run = || -> std::io::Result<()> {
                let mut wav = WavCapture::create(&out, "out")?;
                let r = pdm_eth::session::capture(link.as_mut(), seconds, Some(&mut wav))?;
                wav.finish()?;
                let st = r.status;
                println!("{} frames per channel written to {}", r.written, out.display());
                println!("lost {} duplicates {} resyncs {}; board: underrun {} sd_ovf {} tx_late {} rx_foreign {} rx_bad {}",
                         r.lost, r.duplicates, r.resyncs, st.underrun_count, st.sd_ovf_count, st.tx_late,
                         st.rx_foreign, st.rx_bad);
                if let Some(ppm) = r.clock_ppm {
                    println!("board clock {ppm:+.1} ppm against the PC");
                }
                Ok(())
            };
            match run() {
                Ok(()) => ExitCode::SUCCESS,
                Err(e) => {
                    eprintln!("error: {e}");
                    ExitCode::from(2)
                }
            }
        }
    }
}
