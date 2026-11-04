// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! The window, layout C of docs/screen_mockup.html (design 7.2): one channel
//! large (input on top, board output below, difference strip), a side panel
//! with the live numbers, a toolbar with the channel selector. A network
//! thread owns the link and feeds the UI thread over a channel.

use std::collections::VecDeque;
use std::sync::Arc;
use std::sync::mpsc::{Receiver, Sender};
use std::time::{Duration, Instant};

use eframe::egui::{self, Color32, ColorImage, Rect, Sense, TextureHandle, TextureOptions, pos2, vec2};

use crate::latency::LatencyMeter;
use crate::link::{now_ns, Link};
use crate::pacing::Pacer;
use crate::wavin::InputSet;
use std::sync::atomic::{AtomicUsize, Ordering};
use crate::proto::{decode, encode, Body, FrameType, Header, Status, BOARD_MAC, CMD_RESET_COUNTERS, VERSION};
use crate::rx::OutStream;
use crate::spectro::{diff_column, Spectrogram, COLUMNS};

/// Fixed comparison number (pru-simulator PDM demo report, 2026-11-03:
/// 0.09 s of audio per minute of simulation).
pub const SIM_TEXT: &str = "about 11 min of simulation per second of audio";
const BINS: usize = 129;
/// Clip buffer / PC input ring size (frames).
const RING_FRAMES: u64 = 16384;

/// Numbers from the network thread.
#[derive(Clone, Debug, Default)]
pub struct NetStats {
    pub status: Status,
    pub lost: u64,
    pub duplicates: u64,
    pub resyncs: u64,
    pub clock_ppm: Option<f64>,
    pub out_frames_per_s: f64,
    pub kernel_drops: u64,
    /// Input stream (None without input files).
    pub input: Option<InStats>,
}

/// Input-side numbers.
#[derive(Clone, Debug, Default)]
pub struct InStats {
    pub latency_ms: Option<f64>,
    /// Output index k carries input index k + lag.
    pub lag: Option<i64>,
    pub ahead_frames: Option<i64>,
    pub in_frames_per_s: f64,
    pub underruns: u64,
    pub overwrite_blocked: u64,
    pub restarts: u64,
}

/// What the network thread streams into the board: the files loop.
pub struct NetInput {
    pub set: InputSet,
    pub lead: u32,
    /// Channel of the latency measurement (the UI's selection).
    pub sel: Arc<AtomicUsize>,
    /// Aligned input/output blocks to a shared-memory ring (processing seam).
    pub export: Option<crate::seam::ShmExport>,
}

/// Network thread -> UI.
pub enum NetMsg {
    /// Contiguous output PCM frames.
    Out(Vec<(u64, [i32; 8])>),
    /// Input frames as sent (board index).
    In(Vec<(u64, [i16; 8])>),
    Stats(NetStats),
    Error(String),
}

fn hello(link: &mut dyn Link) -> std::io::Result<()> {
    let h = Header { version: VERSION, ftype: FrameType::Control, length: 0, seq: 0, ts: now_ns() };
    let b = Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 };
    let mac = link.mac();
    link.send(&encode(BOARD_MAC, mac, &h, &b, 60))
}

/// Run the receive side until the UI goes away: say hello (again after
/// 1 s without audio, so a reset board learns the PC MAC again), rebuild
/// the stream, send PCM and stats to the UI about every 20 ms.
pub fn run_net(mut link: Box<dyn Link>, input: Option<NetInput>, tx: Sender<NetMsg>,
               repaint: Arc<dyn Fn() + Send + Sync>) {
    let mut out = OutStream::new();
    let mut pacer = input.as_ref().map(|i| Pacer::new(i.lead, 16384));
    let mut meter = LatencyMeter::new();
    let mut meter_ch = usize::MAX;
    let (mut pos, mut seq) = (0usize, 0u32);
    let mut win_in: VecDeque<u64> = VecDeque::new();
    let mut sent_in: Vec<(u64, [i16; 8])> = Vec::new();
    let mut input = input;
    let mut aligner = crate::seam::Aligner::new();
    let mut last_audio = 0u64;
    let mut last_hello = 0u64;
    let mut last_flush = now_ns();
    let mut win: VecDeque<u64> = VecDeque::new();
    loop {
        let now = now_ns();
        if now.saturating_sub(last_audio) > 1_000_000_000 && now.saturating_sub(last_hello) > 1_000_000_000 {
            if let Err(e) = hello(link.as_mut()) {
                let _ = tx.send(NetMsg::Error(e.to_string()));
                return;
            }
            last_hello = now;
        }
        match link.recv(Duration::from_millis(10)) {
            Ok(Some(f)) => {
                if let Ok((h, Body::AudioOut { status, pcm }, _)) = decode(&f.data) {
                    out.push(&status, &pcm, h.ts, f.t_ns);
                    last_audio = f.t_ns;
                    win.push_back(f.t_ns);
                    if let (Some(p), Some(inp)) = (pacer.as_mut(), input.as_ref()) {
                        p.on_status(&status, f.t_ns);
                        let ch = inp.sel.load(Ordering::Relaxed).min(7);
                        if ch != meter_ch {
                            meter = LatencyMeter::new();
                            meter_ch = ch;
                        }
                        meter.push_status(&status);
                        let y: Vec<i32> = pcm.iter().map(|r| r[ch]).collect();
                        meter.push_out(status.first_index as u64, &y, h.ts);
                    }
                }
            }
            Ok(None) => {}
            Err(e) => {
                let _ = tx.send(NetMsg::Error(e.to_string()));
                return;
            }
        }
        if let (Some(p), Some(inp)) = (pacer.as_mut(), input.as_ref()) {
            let n = inp.set.frames().max(1);
            while let Some(first) = p.next_due(now_ns()) {
                let mut pcm = Box::new([[0i16; 8]; 64]);
                for (k, row) in pcm.iter_mut().enumerate() {
                    *row = inp.set.frame((pos + k) % n);
                    sent_in.push((first as u64 + k as u64, *row));
                }
                let x: Vec<i16> = pcm.iter().map(|r| r[meter_ch.min(7)]).collect();
                meter.push_in(first as u64, &x);
                let h = Header { version: VERSION, ftype: FrameType::AudioIn, length: 0, seq, ts: now_ns() };
                let mac = link.mac();
                if let Err(e) = link.send(&encode(BOARD_MAC, mac, &h, &Body::AudioIn { first_index: first, pcm }, 0)) {
                    let _ = tx.send(NetMsg::Error(e.to_string()));
                    return;
                }
                win_in.push_back(now_ns());
                seq += 1;
                pos = (pos + 64) % n;
            }
        }
        let now = now_ns();
        while win.front().is_some_and(|t| now - t > 1_000_000_000) {
            win.pop_front();
        }
        while win_in.front().is_some_and(|t| now - t > 1_000_000_000) {
            win_in.pop_front();
        }
        if now - last_flush > 20_000_000 {
            last_flush = now;
            let frames = out.take_new();
            if let Some(x) = input.as_mut().and_then(|i| i.export.as_mut())
                && let Some(l) = meter.lag() {
                    aligner.set_lag(l);
                    aligner.push_in(&sent_in);
                    aligner.push_out(&frames);
                    for (i, a, b) in aligner.take_blocks() {
                        if let Err(e) = x.write(i, &a, &b) {
                            let _ = tx.send(NetMsg::Error(format!("shm export: {e}")));
                        }
                    }
                }
            if !frames.is_empty() && tx.send(NetMsg::Out(frames)).is_err() {
                return;
            }
            if !sent_in.is_empty() && tx.send(NetMsg::In(std::mem::take(&mut sent_in))).is_err() {
                return;
            }
            let st = NetStats {
                status: out.last_status().copied().unwrap_or_default(),
                lost: out.lost_frames,
                duplicates: out.duplicates,
                resyncs: out.resyncs,
                clock_ppm: out.board_clock_ppm(),
                out_frames_per_s: win.len() as f64,
                kernel_drops: link.stats().rx_kernel_drops,
                input: pacer.as_ref().map(|p| InStats {
                    latency_ms: meter.board_latency_ms(),
                    lag: meter.lag(),
                    ahead_frames: p.ahead_frames(now),
                    in_frames_per_s: win_in.len() as f64,
                    underruns: p.underruns,
                    overwrite_blocked: p.overwrite_blocked,
                    restarts: p.restarts,
                }),
            };
            if tx.send(NetMsg::Stats(st)).is_err() {
                return;
            }
            repaint();
        }
    }
}

/// Colour of a dB value in `range` (dark blue - purple - orange - yellow).
pub fn heat(db: f32, (lo, hi): (f32, f32)) -> Color32 {
    const STOPS: [(f32, [u8; 3]); 5] = [
        (0.0, [8, 8, 24]),
        (0.35, [70, 20, 110]),
        (0.6, [200, 60, 60]),
        (0.8, [245, 160, 40]),
        (1.0, [255, 250, 200]),
    ];
    let t = ((db - lo) / (hi - lo)).clamp(0.0, 1.0);
    let i = STOPS.iter().position(|(p, _)| *p >= t).unwrap_or(4).max(1);
    let (p0, c0) = STOPS[i - 1];
    let (p1, c1) = STOPS[i];
    let f = (t - p0) / (p1 - p0);
    let m = |a: u8, b: u8| (a as f32 + (b as f32 - a as f32) * f) as u8;
    Color32::from_rgb(m(c0[0], c1[0]), m(c0[1], c1[1]), m(c0[2], c1[2]))
}

/// Colour of a dB difference: blue (output louder) - black - red (input louder).
pub fn diff_heat(d: f32) -> Color32 {
    let t = (d / 30.0).clamp(-1.0, 1.0);
    if t >= 0.0 {
        Color32::from_rgb((255.0 * t) as u8, (60.0 * t) as u8, 20)
    } else {
        Color32::from_rgb(20, (90.0 * -t) as u8, (255.0 * -t) as u8)
    }
}

/// Image of the last `COLUMNS` columns, newest on the right, 8 kHz on top.
pub fn columns_image<'a, I>(cols: I, n: usize, colour: impl Fn(f32) -> Color32) -> ColorImage
where
    I: Iterator<Item = &'a Vec<f32>>,
{
    let mut px = vec![Color32::from_rgb(8, 8, 24); COLUMNS * BINS];
    let x0 = COLUMNS - n.min(COLUMNS);
    for (x, col) in cols.enumerate() {
        for (b, v) in col.iter().enumerate().take(BINS) {
            px[(BINS - 1 - b) * COLUMNS + x0 + x] = colour(*v);
        }
    }
    ColorImage::new([COLUMNS, BINS], px)
}

/// The application.
pub struct App {
    rx: Receiver<NetMsg>,
    pub(crate) out: Vec<Spectrogram>,
    pub(crate) inp: Option<Vec<Spectrogram>>,
    pub(crate) sel: usize,
    tex: [Option<TextureHandle>; 3],
    dirty: bool,
    frames: VecDeque<Instant>,
    pub(crate) stats: NetStats,
    error: Option<String>,
    source: String,
    /// Extra side-panel lines.
    pub(crate) extra: Vec<(String, String)>,
    sel_shared: Option<Arc<AtomicUsize>>,
    /// Input frames waiting until the board plays them, so that the IN and
    /// OUT panes (and the difference) line up on one timeline.
    pending_in: VecDeque<(u64, [i16; 8])>,
    out_newest: Option<u64>,
    shot: Option<(std::path::PathBuf, Instant, bool)>,
    shot_done: Arc<std::sync::atomic::AtomicBool>,
    shot_focused: bool,
}

impl App {
    /// `source`: shown in the toolbar (interface name or "emulator").
    pub fn new(rx: Receiver<NetMsg>, source: String) -> Self {
        App {
            rx,
            out: (0..8).map(|_| Spectrogram::new(256, 128, -90.0, 0.0, (1 << 23) as f32)).collect(),
            inp: None,
            sel: 0,
            tex: [None, None, None],
            dirty: true,
            frames: VecDeque::new(),
            stats: NetStats::default(),
            error: None,
            source,
            extra: Vec::new(),
            sel_shared: None,
            pending_in: VecDeque::new(),
            out_newest: None,
            shot: None,
            shot_done: Arc::new(std::sync::atomic::AtomicBool::new(false)),
            shot_focused: false,
        }
    }

    /// Share the channel selection with the network thread (latency).
    pub fn with_selection(mut self, sel: Arc<AtomicUsize>) -> Self {
        self.sel_shared = Some(sel);
        self
    }

    /// Save one PNG of the window `after` the start, then close it.
    pub fn with_screenshot(mut self, path: std::path::PathBuf, after: Duration) -> Self {
        self.shot = Some((path, Instant::now() + after, false));
        self
    }

    fn screenshot(&mut self, ctx: &egui::Context) {
        let Some((path, at, asked)) = &mut self.shot else { return };
        if self.shot_done.load(std::sync::atomic::Ordering::SeqCst) {
            ctx.send_viewport_cmd(egui::ViewportCommand::Close);
        } else if !self.shot_focused {
            self.shot_focused = true;
            ctx.send_viewport_cmd(egui::ViewportCommand::Focus);
        } else if !*asked && Instant::now() >= *at {
            *asked = true;
            let (path, done) = (path.clone(), self.shot_done.clone());
            ctx.send_viewport_cmd(egui::ViewportCommand::Screenshot(egui::ScreenshotCallback::new(
                move |img: Arc<ColorImage>| {
                    if let Err(e) = save_png(&path, &img) {
                        eprintln!("screenshot: {e}");
                    }
                    done.store(true, std::sync::atomic::Ordering::SeqCst);
                },
            )));
        }
    }

    fn drain(&mut self) {
        while let Ok(m) = self.rx.try_recv() {
            match m {
                NetMsg::Out(v) => {
                    if let (Some((k0, _)), Some(old)) = (v.first(), self.out_newest)
                        && k0 + RING_FRAMES < old {
                            self.pending_in.clear(); // board reset: drop the old run's input
                        }
                    if let Some((k, _)) = v.last() {
                        self.out_newest = Some(*k);
                    }
                    for (c, sp) in self.out.iter_mut().enumerate() {
                        let x: Vec<f32> = v.iter().map(|(_, f)| f[c] as f32).collect();
                        sp.push(&x);
                    }
                    self.dirty = true;
                }
                NetMsg::In(v) => {
                    self.inp.get_or_insert_with(|| {
                        (0..8).map(|_| Spectrogram::new(256, 128, -90.0, 0.0, 32768.0)).collect()
                    });
                    self.pending_in.extend(v);
                }
                NetMsg::Stats(s) => {
                    self.extra = match &s.input {
                        Some(i) => vec![
                            ("board audio latency".into(), i.latency_ms.map_or("-".into(), |m| format!("{m:.2} ms"))),
                            ("buffered ahead (PC)".into(),
                             i.ahead_frames.map_or("-".into(), |a| format!("{:.2} s", a as f64 / 16000.0))),
                            ("frames/s in".into(), format!("{:.0}", i.in_frames_per_s)),
                            ("input underrun / blocked / restart".into(),
                             format!("{} / {} / {}", i.underruns, i.overwrite_blocked, i.restarts)),
                        ],
                        None => Vec::new(),
                    };
                    self.stats = s;
                }
                NetMsg::Error(e) => self.error = Some(e),
            }
        }
    }

    /// Move input frames the board has played (index <= newest output
    /// index + lag) into the input spectrograms.
    fn release_input(&mut self) {
        let (Some(inp), Some(k)) = (self.inp.as_mut(), self.out_newest) else { return };
        let lag = self.stats.input.as_ref().and_then(|i| i.lag).unwrap_or(0);
        let limit = k as i64 + lag;
        // input more than two rings ahead of the output can never play (stale
        // after a pacer restart): drop it from the head
        while self.pending_in.front().is_some_and(|(i, _)| *i as i64 > limit + 2 * RING_FRAMES as i64) {
            self.pending_in.pop_front();
        }
        let n = self.pending_in.iter().take_while(|(i, _)| (*i as i64) <= limit).count();
        if n == 0 {
            return;
        }
        let v: Vec<(u64, [i16; 8])> = self.pending_in.drain(..n).collect();
        for (c, sp) in inp.iter_mut().enumerate() {
            let x: Vec<f32> = v.iter().map(|(_, f)| f[c] as f32).collect();
            sp.push(&x);
        }
        self.dirty = true;
    }

    fn fps(&mut self) -> f64 {
        let now = Instant::now();
        self.frames.push_back(now);
        while self.frames.front().is_some_and(|t| now - *t > Duration::from_secs(1)) {
            self.frames.pop_front();
        }
        self.frames.len() as f64
    }

    fn update_textures(&mut self, ctx: &egui::Context) {
        if !self.dirty {
            return;
        }
        self.dirty = false;
        let o = &self.out[self.sel];
        let img_out = columns_image(o.columns().iter(), o.columns().len(), |v| heat(v, o.range()));
        let (img_in, img_diff) = match &self.inp {
            Some(inp) => {
                let i = &inp[self.sel];
                let n = i.columns().len().min(o.columns().len());
                let a: Vec<&Vec<f32>> = i.columns().iter().rev().take(n).rev().collect();
                let b: Vec<&Vec<f32>> = o.columns().iter().rev().take(n).rev().collect();
                let d: Vec<Vec<f32>> = a.iter().zip(&b).map(|(x, y)| diff_column(x, y)).collect();
                (Some(columns_image(i.columns().iter(), i.columns().len(), |v| heat(v, i.range()))),
                 Some(columns_image(d.iter(), d.len(), diff_heat)))
            }
            None => (None, None),
        };
        for (slot, img, name) in [(0, img_in, "in"), (1, Some(img_out), "out"), (2, img_diff, "diff")] {
            match (img, &mut self.tex[slot]) {
                (Some(img), Some(t)) => t.set(img, TextureOptions::LINEAR),
                (Some(img), t @ None) => *t = Some(ctx.load_texture(name, img, TextureOptions::LINEAR)),
                (None, _) => {}
            }
        }
    }

    /// Side-panel lines.
    pub fn side_lines(&self, fps: f64) -> Vec<(String, String)> {
        let s = &self.stats;
        let st = &s.status;
        let mut v = vec![
            ("spectrogram".into(), format!("{fps:.0} fps")),
            ("frames/s out".into(), format!("{:.0}", s.out_frames_per_s)),
        ];
        v.extend(self.extra.iter().cloned());
        v.extend([
            ("lost / dup / resync".into(), format!("{} / {} / {}", s.lost, s.duplicates, s.resyncs)),
            ("UNDERRUN / SD_OVF".into(), format!("{} / {}", st.underrun_count, st.sd_ovf_count)),
            ("tx_late".into(), st.tx_late.to_string()),
            ("PC kernel drops".into(), s.kernel_drops.to_string()),
            ("board clock".into(), s.clock_ppm.map_or("-".into(), |p| format!("{p:+.1} ppm"))),
            ("HALF_SEQ".into(), st.half_seq.to_string()),
            ("simulator".into(), SIM_TEXT.into()),
        ]);
        v
    }
}

/// Write an egui image as an 8-bit RGBA PNG.
pub fn save_png(path: &std::path::Path, img: &ColorImage) -> std::io::Result<()> {
    let f = std::io::BufWriter::new(std::fs::File::create(path)?);
    let mut enc = png::Encoder::new(f, img.size[0] as u32, img.size[1] as u32);
    enc.set_color(png::ColorType::Rgba);
    enc.set_depth(png::BitDepth::Eight);
    let mut w = enc.write_header().map_err(std::io::Error::other)?;
    let data: Vec<u8> = img.pixels.iter().flat_map(|c| c.to_array()).collect();
    w.write_image_data(&data).map_err(std::io::Error::other)
}

fn pane(ui: &mut egui::Ui, h: f32, tex: &Option<TextureHandle>, label: &str, empty: &str) {
    let (resp, painter) = ui.allocate_painter(vec2(ui.available_width(), h), Sense::hover());
    let r = resp.rect;
    painter.rect_filled(r, 2.0, Color32::from_rgb(8, 8, 24));
    match tex {
        Some(t) => {
            painter.image(t.id(), r, Rect::from_min_max(pos2(0.0, 0.0), pos2(1.0, 1.0)), Color32::WHITE);
        }
        None => {
            painter.text(r.center(), egui::Align2::CENTER_CENTER, empty, egui::FontId::proportional(16.0),
                         Color32::GRAY);
        }
    }
    painter.text(r.left_top() + vec2(6.0, 4.0), egui::Align2::LEFT_TOP, label,
                 egui::FontId::proportional(14.0), Color32::from_rgb(230, 230, 230));
}

impl eframe::App for App {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = ui.ctx().clone();
        self.drain();
        self.release_input();
        let fps = self.fps();
        self.update_textures(&ctx);
        egui::Panel::top("toolbar").show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.strong("PDM audio over Ethernet");
                ui.separator();
                ui.label("channel");
                for c in 0..8 {
                    if ui.selectable_label(self.sel == c, format!("ch{c}")).clicked() && self.sel != c {
                        self.sel = c;
                        self.dirty = true;
                        if let Some(s) = &self.sel_shared {
                            s.store(c, Ordering::Relaxed);
                        }
                    }
                }
                ui.separator();
                ui.label(&self.source);
                if let Some(e) = &self.error {
                    ui.colored_label(Color32::RED, e);
                }
            });
        });
        egui::Panel::right("numbers").resizable(false).exact_size(320.0).show(ui, |ui| {
            ui.add_space(6.0);
            for (k, v) in self.side_lines(fps) {
                ui.label(egui::RichText::new(k).small().weak());
                ui.add(egui::Label::new(egui::RichText::new(v).size(18.0).strong()).wrap());
                ui.add_space(4.0);
            }
        });
        egui::CentralPanel::default().show(ui, |ui| {
            let h = ((ui.available_height() - 60.0) / 2.0).max(60.0);
            let c = self.sel;
            pane(ui, h, &self.tex[0], &format!("ch{c} IN (PC)"), "no input stream");
            pane(ui, h, &self.tex[1], &format!("ch{c} OUT (board, PDM round trip)"), "waiting for the board");
            pane(ui, 40.0, &self.tex[2], "difference IN - OUT", "");
        });
        self.screenshot(&ctx);
        ctx.request_repaint_after(Duration::from_millis(16));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn heat_ends_and_clamps() {
        assert_eq!(heat(-90.0, (-90.0, 0.0)), Color32::from_rgb(8, 8, 24));
        assert_eq!(heat(0.0, (-90.0, 0.0)), Color32::from_rgb(255, 250, 200));
        assert_eq!(heat(20.0, (-90.0, 0.0)), heat(0.0, (-90.0, 0.0)));
    }

    #[test]
    fn columns_image_puts_newest_right_and_low_bins_bottom() {
        let cols = vec![vec![0.0f32; BINS]; 3];
        let img = columns_image(cols.iter(), 3, |v| if v == 0.0 { Color32::WHITE } else { Color32::BLACK });
        assert_eq!(img.size, [COLUMNS, BINS]);
        assert_eq!(img.pixels[(BINS - 1) * COLUMNS + COLUMNS - 1], Color32::WHITE);
        assert_eq!(img.pixels[(BINS - 1) * COLUMNS + COLUMNS - 4], Color32::from_rgb(8, 8, 24));
    }

    #[test]
    fn net_thread_feeds_output_from_emulator() {
        use crate::emulator::{EmuConfig, Emulator};
        let (tx, rx) = std::sync::mpsc::channel();
        let link: Box<dyn Link> = Box::new(Emulator::new(EmuConfig::default()));
        std::thread::spawn(move || run_net(link, None, tx, Arc::new(|| {})));
        let mut app = App::new(rx, "emulator".into());
        std::thread::sleep(Duration::from_millis(400));
        app.drain();
        assert!(app.out[3].columns().len() > 20, "{}", app.out[3].columns().len());
        assert!(app.stats.out_frames_per_s > 100.0, "{}", app.stats.out_frames_per_s);
        assert!(app.side_lines(60.0).iter().any(|(k, v)| k == "simulator" && v == SIM_TEXT));
        assert!(app.inp.is_none());
    }

    #[test]
    fn input_pane_resumes_after_board_reset() {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into());
        let out = |i0: u64| NetMsg::Out((i0..i0 + 640).map(|i| (i, [0; 8])).collect());
        let inp = |i0: u64| NetMsg::In((i0..i0 + 640).map(|i| (i, [1000; 8])).collect());
        // before the reset: input runs 8000 ahead of the output, as always
        tx.send(inp(108_000)).unwrap();
        tx.send(out(100_000)).unwrap();
        app.drain();
        app.release_input();
        let n0 = app.inp.as_ref().unwrap()[0].columns().len();
        // board reset: output restarts near 0, the pacer restarts the input there
        tx.send(out(0)).unwrap();
        tx.send(inp(0)).unwrap();
        app.drain();
        app.release_input();
        let n1 = app.inp.as_ref().unwrap()[0].columns().len();
        assert!(n1 > n0, "input pane frozen after the reset ({n0} -> {n1} columns)");
        assert!(app.pending_in.len() < 1000, "{} stale frames kept", app.pending_in.len());
    }

    #[test]
    fn net_thread_with_input_shows_latency() {
        use crate::emulator::{EmuConfig, Emulator};
        let (tx, rx) = std::sync::mpsc::channel();
        let link: Box<dyn Link> = Box::new(Emulator::new(EmuConfig::default()));
        let mut set = InputSet::default();
        let mut s = 7u32;
        set.ch[0] = (0..32000).map(|_| { s = s.wrapping_mul(1_664_525).wrapping_add(1_013_904_223); (s >> 17) as i16 }).collect();
        let sel = Arc::new(AtomicUsize::new(0));
        let input = NetInput { set, lead: 8000, sel: sel.clone(), export: None };
        std::thread::spawn(move || run_net(link, Some(input), tx, Arc::new(|| {})));
        let mut app = App::new(rx, "emulator".into()).with_selection(sel);
        std::thread::sleep(Duration::from_millis(1800));
        app.drain();
        app.release_input();
        let i = app.stats.input.clone().expect("input stats");
        let ms = i.latency_ms.expect("latency");
        assert!((ms - 2.5).abs() < 0.3, "{ms} ms");
        assert!(app.inp.as_ref().unwrap()[0].columns().len() > 50);
        assert_eq!(i.underruns, 0);
        assert!(app.side_lines(60.0).iter().any(|(k, _)| k == "board audio latency"));
    }
}
