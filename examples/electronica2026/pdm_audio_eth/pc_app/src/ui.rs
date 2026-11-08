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
use crate::sources::{Source, BUILTIN};
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
    /// Loop offsets of the playlist's pass starts (empty without a playlist).
    pub marks: Vec<usize>,
}

/// A speaker icon button `size` high; `on` = highlighted with sound waves.
fn speaker_toggle(ui: &mut egui::Ui, on: bool, size: f32) -> egui::Response {
    let (r, resp) = ui.allocate_exact_size(vec2(size, size), Sense::click());
    let v = ui.style().interact_selectable(&resp, on);
    let p = ui.painter();
    if on || resp.hovered() {
        p.rect_filled(r, 4.0, v.weak_bg_fill);
    }
    let c = v.fg_stroke.color;
    let (x, y, u) = (r.left() + size * 0.18, r.center().y, size / 10.0);
    p.rect_filled(Rect::from_min_max(pos2(x, y - 1.4 * u), pos2(x + 1.8 * u, y + 1.4 * u)), 0.0, c);
    p.add(egui::Shape::convex_polygon(
        vec![pos2(x + 1.8 * u, y - 1.4 * u), pos2(x + 4.2 * u, y - 3.4 * u), pos2(x + 4.2 * u, y + 3.4 * u), pos2(x + 1.8 * u, y + 1.4 * u)],
        c, egui::Stroke::NONE));
    if on {
        for k in [1.0f32, 2.0] {
            let rad = (1.6 + 1.4 * k) * u;
            let pts: Vec<_> = (-4..=4).map(|i| {
                let a = i as f32 * 0.16;
                pos2(x + 4.6 * u + rad * a.cos() - 1.4 * u, y + rad * a.sin())
            }).collect();
            p.add(egui::Shape::line(pts, egui::Stroke::new(u * 0.8, c)));
        }
    }
    resp
}

/// Most channel audio handed to the player per frame (0.15 s at 16 kHz).
const LISTEN_MAX: usize = 2400;

/// What the PC's audio output plays.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Listen {
    Off,
    /// The board output of one channel (the noisy mic capture).
    Channel(usize),
    /// The separated stream of the talker nearest this azimuth (audio_pipe).
    Talker(f32),
}

/// One board output sample (24 bit) for the player: at the scene's file
/// level x 0.9 FS when the pass scale is known, else the top 16 bits.
fn channel_sample(x: i32, scale: Option<f32>) -> i16 {
    match scale {
        Some(s) if s > 0.0 => (x as f32 * s * 32767.0 * 0.9).clamp(-32768.0, 32767.0) as i16,
        _ => (x >> 8).clamp(-32768, 32767) as i16,
    }
}

/// UI -> network thread.
pub enum NetCmd {
    /// New samples for one channel (the source selector, 16 kHz, 0.5 FS);
    /// starts the stream if none runs (design 7.2a).
    SetChannel { ch: usize, samples: Vec<i16> },
    /// A capture set: all its channels start on the same sample.
    SetSet { chans: Vec<(usize, Vec<i16>)> },
    /// A new playlist (an opened scene): all 8 channels and the pass starts
    /// (loop offsets); it plays from its first frame with the next block.
    SetPlaylist { set: InputSet, marks: Vec<usize>, generation: u64 },
}

/// What the network thread needs to start a stream later (on a source choice).
pub struct NetCtl {
    pub cmds: Receiver<NetCmd>,
    /// Pacer lead of a stream started by a source choice.
    pub lead: u32,
    /// Channel of the latency measurement (the UI's selection).
    pub sel: Arc<AtomicUsize>,
}

impl NetCtl {
    /// No commands will come (tests, screenshots of a fixed set).
    pub fn idle() -> Self {
        let (_tx, cmds) = std::sync::mpsc::channel();
        NetCtl { cmds, lead: 8000, sel: Arc::new(AtomicUsize::new(0)) }
    }
}

/// Network thread -> UI.
pub enum NetMsg {
    /// Contiguous output PCM frames.
    Out(Vec<(u64, [i32; 8])>),
    /// Input frames as sent (board index).
    In(Vec<(u64, [i16; 8])>),
    Stats(NetStats),
    Error(String),
    /// The frame at loop offset `marks[slot]` went to the board with board
    /// index `index`, in loop number `cycle` (from 0), of playlist number
    /// `generation` (0 = the start-up playlist, +1 per `SetPlaylist`).
    Mark { slot: usize, cycle: u64, index: u64, generation: u64 },
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
pub fn run_net(mut link: Box<dyn Link>, input: Option<NetInput>, ctl: NetCtl, tx: Sender<NetMsg>,
               repaint: Arc<dyn Fn() + Send + Sync>) {
    let mut out = OutStream::new();
    let mut pacer = input.as_ref().map(|i| Pacer::new(i.lead, 16384));
    let mut meter = LatencyMeter::new();
    let mut meter_ch = usize::MAX;
    // frames sent so far; every channel loops on its own length (design 7.2a)
    let (mut pos, mut seq) = (0u64, 0u32);
    let mut win_in: VecDeque<u64> = VecDeque::new();
    let mut sent_in: Vec<(u64, [i16; 8])> = Vec::new();
    // playlist number of the marks (NetCmd::SetPlaylist)
    let mut generation = 0u64;
    let mut input = input;
    let mut aligner = crate::seam::Aligner::new();
    // the export keeps the last lag found: the meter forgets it over silence
    // (a playlist gap), and the ring must not get a hole there
    let mut export_lag: Option<i64> = None;
    let mut last_audio = 0u64;
    let mut last_hello = 0u64;
    let mut last_flush = now_ns();
    let mut win: VecDeque<u64> = VecDeque::new();
    loop {
        while let Ok(cmd) = ctl.cmds.try_recv() {
            let inp = input.get_or_insert_with(|| {
                let mut set = InputSet::default();
                set.fill_with_booth();
                pacer = Some(Pacer::new(ctl.lead, 16384));
                NetInput { set, lead: ctl.lead, sel: ctl.sel.clone(), export: None, marks: Vec::new() }
            });
            // a new source plays from its first sample, starting with the next block
            match cmd {
                NetCmd::SetChannel { ch, samples } => inp.set.set_channel(ch, samples, pos),
                NetCmd::SetSet { chans } => {
                    for (ch, samples) in chans {
                        inp.set.set_channel(ch, samples, pos);
                    }
                }
                NetCmd::SetPlaylist { mut set, marks, generation: g } => {
                    for c in 0..8 {
                        inp.set.set_channel(c, std::mem::take(&mut set.ch[c]), pos);
                    }
                    inp.marks = marks;
                    generation = g;
                }
            }
        }
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
            while let Some(first) = p.next_due(now_ns()) {
                let mut pcm = Box::new([[0i16; 8]; 64]);
                let total = inp.set.ch[0].len() as u64;
                for (k, row) in pcm.iter_mut().enumerate() {
                    let p = pos + k as u64;
                    *row = inp.set.frame_looped(p);
                    sent_in.push((first as u64 + k as u64, *row));
                    if !inp.marks.is_empty() && total > 0 {
                        let rel = p.saturating_sub(inp.set.start[0]);
                        if let Some(slot) = inp.marks.iter().position(|&m| m as u64 == rel % total) {
                            let m = NetMsg::Mark { slot, cycle: rel / total, index: first as u64 + k as u64, generation };
                            if tx.send(m).is_err() {
                                return;
                            }
                        }
                    }
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
                pos += 64;
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
            export_lag = meter.lag().or(export_lag);
            if let Some(x) = input.as_mut().and_then(|i| i.export.as_mut())
                && let Some(l) = export_lag {
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
    /// Commands to the network thread (source selector).
    cmds: Option<Sender<NetCmd>>,
    /// What each channel plays.
    pub(crate) sources: [Source; 8],
    /// The loaded capture set, for the side panel.
    pub(crate) set_info: Option<String>,
    /// Playlist scene names, by pass slot.
    pub(crate) scenes: Vec<String>,
    /// Scene and pass number of the newest pass start sent to the board.
    pub(crate) pass: Option<(String, u64)>,
    /// Text lanes per talker.
    pub(crate) lanes: crate::lanes::Lanes,
    /// audio_pipe's state; None without --transcribe.
    pub(crate) tstatus: Option<crate::transcribe::TStatus>,
    /// End of speech to final text of the newest utterance.
    pub(crate) text_latency_ms: Option<f32>,
    /// The audio_pipe child (--transcribe), output scale per pass slot, transcript log.
    tpipe: Option<crate::transcribe::TextPipe>,
    pass_scales: Vec<f32>,
    tlog: Option<std::fs::File>,
    /// What is played on the PC's audio output (one source at a time).
    pub(crate) listen: Listen,
    player: Option<crate::player::Player>,
    player_cmd: Vec<String>,
    /// The player could not start (missing, no device).
    player_failed: bool,
    /// Board output x this = the current pass's file level (playlist).
    cur_scale: Option<f32>,
    /// Playlist number (marks of an older one are ignored) and, after "open
    /// scene…", the board index from which text belongs to the new scene
    /// (u64::MAX until its first mark arrives).
    generation: u64,
    text_from: Option<u64>,
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
            cmds: None,
            sources: std::array::from_fn(|_| Source::Booth),
            set_info: None,
            scenes: Vec::new(),
            pass: None,
            lanes: crate::lanes::Lanes::default(),
            tstatus: None,
            text_latency_ms: None,
            tpipe: None,
            pass_scales: Vec::new(),
            tlog: None,
            listen: Listen::Off,
            player: None,
            player_cmd: crate::player::player_cmd(),
            player_failed: false,
            cur_scale: None,
            generation: 0,
            text_from: None,
        }
    }

    /// Text lanes from `tp` (`audio_pipe live`); `scales`: board output to file level, by pass slot.
    pub fn with_transcription(mut self, tp: crate::transcribe::TextPipe, scales: Vec<f32>, log: Option<std::fs::File>) -> Self {
        self.tstatus = Some(tp.status().clone());
        self.tpipe = Some(tp);
        self.pass_scales = scales;
        self.tlog = log;
        self
    }

    fn log_line(&mut self, line: &str) {
        if let Some(f) = &mut self.tlog {
            use std::io::Write;
            if writeln!(f, "{line}").is_err() {
                self.tlog = None;
            }
        }
    }

    /// New text from audio_pipe, its status.
    fn poll_text(&mut self) {
        let Some(tp) = &mut self.tpipe else { return };
        let lines = tp.poll(Instant::now());
        self.tstatus = Some(tp.status().clone());
        for t in lines {
            if let Ok(j) = serde_json::to_string(&t) {
                self.log_line(&j);
            }
            self.on_text(t);
            self.dirty = true;
        }
    }

    /// A final utterance from audio_pipe.
    pub(crate) fn on_text(&mut self, t: crate::transcribe::TextLine) {
        if self.text_from.is_some_and(|from| t.start < from) {
            return; // the replaced scene's last (flushed, cut) sentences
        }
        self.text_latency_ms = Some(t.latency_ms);
        self.lanes.add(t);
    }

    /// Board index (input side) of the newest frame the panes show.
    fn now_index(&self) -> Option<u64> {
        let lag = self.stats.input.as_ref().and_then(|i| i.lag).unwrap_or(0);
        self.out_newest.map(|k| (k as i64 + lag).max(0) as u64)
    }

    /// Height of the lane area.
    fn lanes_h(&self) -> f32 {
        match self.tstatus {
            None => LANE_H,
            Some(_) => LANE_H * self.lanes.lanes.len().clamp(4, crate::lanes::MAX_LANES) as f32,
        }
    }

    /// The text lanes, on the panes' time axis; returns the azimuth of a lane
    /// whose label was clicked (= listen to that talker).
    fn draw_lanes(&self, ui: &mut egui::Ui, h: f32) -> Option<f32> {
        let (resp, painter) = ui.allocate_painter(vec2(ui.available_width(), h), Sense::hover());
        let img = image_rect(resp.rect);
        painter.rect_filled(img, 2.0, Color32::from_rgb(17, 19, 24));
        let grey = Color32::from_rgb(150, 150, 160);
        let msg = match &self.tstatus {
            None => Some("transcription off".to_string()),
            Some(st @ crate::transcribe::TStatus::Off(_)) => Some(format!("transcription {st}")),
            Some(st) if self.lanes.lanes.is_empty() => Some(match st {
                crate::transcribe::TStatus::Running => "listening…".to_string(),
                _ => format!("transcription {st}"),
            }),
            Some(_) => None,
        };
        if let Some(m) = msg {
            painter.text(img.center(), egui::Align2::CENTER_CENTER, m, egui::FontId::proportional(LANE_MSG_PX), grey);
            return None;
        }
        let Some(now) = self.now_index() else { return None };
        let mut clicked = None;
        let clip = painter.with_clip_rect(img);
        let secs = |i: u64| now.saturating_sub(i) as f32 / 16000.0;
        for (row, lane) in self.lanes.lanes.iter().enumerate() {
            let top = img.top() + row as f32 * LANE_H;
            if row > 0 {
                painter.line_segment([pos2(img.left(), top), pos2(img.right(), top)], (1.0, Color32::from_rgb(35, 38, 46)));
            }
            let c = crate::lanes::palette(lane.colour);
            let fill = Color32::from_rgb((c.r() as f32 * 0.5) as u8, (c.g() as f32 * 0.5) as u8, (c.b() as f32 * 0.5) as u8);
            for t in &lane.items {
                let x0 = time_x(img.left(), img.right(), secs(t.start));
                let x1 = time_x(img.left(), img.right(), secs(t.end));
                let g = painter.layout_no_wrap(t.text.clone(), egui::FontId::proportional(LANE_TEXT_PX), Color32::WHITE);
                let w = (g.size().x + 16.0).max(x1 - x0);
                let x0 = chip_x(x0, w, img.right());
                let r = Rect::from_min_max(pos2(x0, top + 4.0), pos2(x0 + w, top + LANE_H - 4.0));
                clip.rect_filled(r, r.height() / 2.0, fill);
                clip.galley(pos2(x0 + 8.0, r.center().y - g.size().y / 2.0), g, Color32::WHITE);
            }
            // the label last, on its own backing: a sentence box scrolling left must not cover it
            let lg = painter.layout_no_wrap(format!("● {} · {:.0}°", lane.label, lane.az), egui::FontId::proportional(LANE_LABEL_PX), c);
            let lr = Rect::from_min_size(pos2(resp.rect.left(), top + (LANE_H - lg.size().y) / 2.0 - 2.0), lg.size() + vec2(8.0, 4.0));
            let hit = ui.interact(lr, ui.scope_id().with(("lane", lane.label)), Sense::click())
                .on_hover_text("hear this talker (separated stream); again = off");
            let on = matches!(self.listen, Listen::Talker(a) if crate::lanes::ang_dist(a, lane.az) <= crate::lanes::REUSE_DEG);
            painter.rect_filled(lr, 4.0, if on || hit.hovered() { Color32::from_rgb(45, 50, 62) } else { Color32::from_rgb(17, 19, 24) });
            if on {
                painter.rect_stroke(lr, 4.0, egui::Stroke::new(1.5, Color32::WHITE), egui::StrokeKind::Inside);
            }
            painter.galley(lr.min + vec2(2.0, 2.0), lg, c);
            if hit.clicked() {
                clicked = Some(lane.az);
            }
        }
        clicked
    }

    /// Scene names of the playlist (`--playlist`), in pass-slot order.
    pub fn with_playlist(mut self, scenes: Vec<String>) -> Self {
        self.scenes = scenes;
        self
    }

    /// Title of the IN pane of channel `c`.
    pub(crate) fn in_label(&self, c: usize) -> String {
        match &self.pass {
            Some((scene, n)) => format!("ch{c} IN (PC): {scene} (pass {n})"),
            None => format!("ch{c} IN (PC): {}", self.sources[c].label()),
        }
    }

    /// Let the source selector reach the network thread.
    pub fn with_commands(mut self, cmds: Sender<NetCmd>) -> Self {
        self.cmds = Some(cmds);
        self
    }

    /// Sources of the channels given on the command line.
    pub fn with_sources(mut self, sources: [Source; 8]) -> Self {
        self.sources = sources;
        self
    }

    fn send_channel(&mut self, ch: usize, samples: Vec<i16>, src: Source) {
        if !self.scenes.is_empty() {
            return; // a playlist runs: its scenes are transcribed, nothing may be mixed in
        }
        if let Some(c) = &self.cmds
            && c.send(NetCmd::SetChannel { ch, samples }).is_ok() {
                self.sources[ch] = src;
                self.error = None;
                self.dirty = true;
            }
    }

    /// Play a built-in source on channel `ch` (design 7.2a).
    pub fn set_source(&mut self, ch: usize, src: Source) {
        match src.samples(ch) {
            Ok(x) => self.send_channel(ch, x, src),
            Err(e) => self.error = Some(e.to_string()),
        }
    }

    /// Play a WAV file on channel `ch`; a file that does not load changes nothing.
    pub fn choose_file(&mut self, ch: usize, path: &std::path::Path) {
        match crate::wavin::load_channel(path) {
            Ok(x) => {
                let name = path.file_name().map_or_else(|| path.display().to_string(), |n| n.to_string_lossy().into_owned());
                self.send_channel(ch, x, Source::File(name));
            }
            Err(e) => self.error = Some(e.to_string()),
        }
    }

    /// The player command (tests: a fake player).
    pub fn with_player_cmd(mut self, cmd: Vec<String>) -> Self {
        self.player_cmd = cmd;
        self
    }

    /// Chooses what to hear; the active choice again = off. Channel audio is
    /// played here, talker audio by audio_pipe.
    pub fn set_listen(&mut self, l: Listen) {
        let l = if l == self.listen { Listen::Off } else { l };
        if !matches!(l, Listen::Channel(_)) {
            self.player = None;
        }
        self.player_failed = false;
        if let Listen::Channel(_) = l
            && self.player.as_ref().is_none_or(|p| !p.alive()) {
                self.player = crate::player::Player::spawn(&self.player_cmd).ok();
                self.player_failed = self.player.is_none();
            }
        // talker audio comes from audio_pipe: tell it what to play (None = stop)
        let az = if let Listen::Talker(a) = l { Some(a) } else { None };
        if let Some(tp) = &mut self.tpipe
            && tp.listening() != az {
                tp.listen(az);
            }
        self.listen = l;
    }

    fn listen_label(&self) -> String {
        let dead = self.player_failed || match self.listen {
            Listen::Channel(_) => self.player.as_ref().is_none_or(|p| !p.alive()),
            // talker audio needs a running audio_pipe whose player works
            Listen::Talker(_) => self.tpipe.as_ref().is_none_or(|tp| matches!(tp.status(), crate::transcribe::TStatus::Off(_)) || tp.listen_ok() == Some(false)),
            Listen::Off => false,
        };
        match self.listen {
            Listen::Off => "off".into(),
            _ if dead => "audio output unavailable".into(),
            Listen::Channel(c) => format!("ch{c} board out"),
            Listen::Talker(az) => match self.lanes.lanes.iter().find(|l| crate::lanes::ang_dist(l.az, az) <= crate::lanes::REUSE_DEG) {
                Some(l) => format!("talker {} · {:.0}°", l.label, l.az),
                None => format!("talker · {az:.0}°"),
            },
        }
    }

    /// Opens a Scene Simulator scene (zip or folder) in place of the playlist:
    /// it loops as a one-scene playlist, transcribed like the booth scenes. A
    /// scene that does not load shows an error and changes nothing.
    pub fn open_scene(&mut self, path: &std::path::Path) {
        let pl = match crate::playlist::load_scene(path) {
            Ok(p) => p,
            Err(e) => {
                self.error = Some(e.to_string());
                return;
            }
        };
        let pass = &pl.passes[0];
        let (name, scale) = (pass.scene.clone(), pass.scale);
        if let Some(c) = &self.cmds
            && c.send(NetCmd::SetPlaylist { set: pl.set, marks: vec![0], generation: self.generation + 1 }).is_ok() {
                self.generation += 1;
                self.text_from = Some(u64::MAX);
                self.scenes = vec![name.clone()];
                self.pass_scales = vec![scale];
                self.pass = None;
                self.lanes = Default::default();
                self.source = format!("scene {name}");
                self.error = None;
                self.dirty = true;
            }
    }

    /// Load up to 8 mono files as one capture set onto ch0, ch1, ... in file
    /// name order (design 7.2a); a set that does not load changes nothing.
    pub fn choose_set(&mut self, mut paths: Vec<std::path::PathBuf>) {
        if !self.scenes.is_empty() {
            return; // a playlist runs (see send_channel)
        }
        paths.sort_by(|a, b| a.file_name().cmp(&b.file_name()));
        let set = match crate::wavin::load_set(&paths) {
            Ok(s) => s,
            Err(e) => {
                self.error = Some(e.to_string());
                return;
            }
        };
        let chans: Vec<(usize, Vec<i16>)> = set.ch.into_iter().enumerate().collect();
        let n = chans.len();
        if let Some(c) = &self.cmds
            && c.send(NetCmd::SetSet { chans }).is_ok() {
                for (k, p) in paths.iter().enumerate() {
                    let name = p.file_name().map_or_else(|| p.display().to_string(), |n| n.to_string_lossy().into_owned());
                    self.sources[k] = Source::SetFile(name);
                }
                let cut = set.trimmed.iter().map(usize::to_string).collect::<Vec<_>>().join(" / ");
                self.set_info = Some(format!("{n} ch, {:.2} s, cut {cut} samples", set.frames as f64 / 16000.0));
                self.error = None;
                self.dirty = true;
            }
    }

    /// The "Sources" table: one selector per channel.
    fn sources_table(&mut self, ui: &mut egui::Ui) {
        ui.label(egui::RichText::new("Sources").size(LABEL_PX).color(LABEL_COLOUR));
        if !self.scenes.is_empty() {
            ui.horizontal(|ui| {
                if ui.button("open scene…").on_hover_text("A Scene Simulator zip: loops in place of the playlist, transcribed").clicked()
                    && let Some(p) = rfd::FileDialog::new().set_title("Scene Simulator zip").add_filter("zip", &["zip", "ZIP"]).pick_file() {
                        self.open_scene(&p);
                    }
                if ui.button("open scene folder…").on_hover_text("A folder with mic1..8.wav (or mics/mic1..8.wav)").clicked()
                    && let Some(p) = rfd::FileDialog::new().set_title("Scene folder").pick_folder() {
                        self.open_scene(&p);
                    }
            });
            ui.label(egui::RichText::new("playlist: sources are locked").size(LABEL_PX));
            ui.disable();
        }
        if ui.button("open capture set...")
            .on_hover_text("Up to 8 mono WAV files of one sample rate: sorted by name onto ch0, ch1, ..., \
                            started on the same sample, trimmed to the shortest, one gain for the set")
            .clicked()
            && let Some(p) = rfd::FileDialog::new()
                .set_title("Capture set: up to 8 mono WAV files")
                .add_filter("WAV audio", &["wav", "WAV"])
                .pick_files()
        {
            self.choose_set(p);
        }
        if let Some(i) = &self.set_info {
            ui.label(egui::RichText::new(format!("capture set: {i}")).size(LABEL_PX));
        }
        let mut pick: Option<(usize, Option<Source>)> = None; // None: open a file
        egui::Grid::new("sources").num_columns(2).spacing([8.0, 4.0]).show(ui, |ui| {
            for c in 0..8 {
                ui.label(egui::RichText::new(format!("ch{c}")).size(LABEL_PX));
                egui::ComboBox::from_id_salt(("source", c))
                    .width(200.0)
                    .selected_text(self.sources[c].label())
                    .show_ui(ui, |ui| {
                        for b in BUILTIN {
                            if ui.selectable_label(self.sources[c] == b, b.label()).clicked() {
                                pick = Some((c, Some(b.clone())));
                            }
                        }
                        ui.separator();
                        if ui.button("open WAV file...").clicked() {
                            pick = Some((c, None));
                        }
                    });
                ui.end_row();
            }
        });
        match pick {
            Some((c, Some(src))) => self.set_source(c, src),
            Some((c, None)) => {
                if let Some(p) = rfd::FileDialog::new()
                    .set_title(format!("WAV file for ch{c}"))
                    .add_filter("WAV audio", &["wav", "WAV"])
                    .pick_file()
                {
                    self.choose_file(c, &p);
                }
            }
            None => {}
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
        let mut heard: Vec<i16> = Vec::new();
        self.drain_msgs(&mut heard);
        // after a stall only the newest LISTEN_MAX goes to the player: late sound would stay late
        if let Some(p) = &self.player
            && !heard.is_empty() {
                p.push(&heard[heard.len().saturating_sub(LISTEN_MAX)..]);
            }
    }

    fn drain_msgs(&mut self, heard: &mut Vec<i16>) {
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
                    if let Listen::Channel(c) = self.listen {
                        heard.extend(v.iter().map(|(_, f)| channel_sample(f[c], self.cur_scale)));
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
                NetMsg::Mark { slot, cycle, index, generation } => {
                    if generation != self.generation {
                        continue; // a mark of the playlist that was replaced
                    }
                    if self.text_from == Some(u64::MAX) {
                        self.text_from = Some(index); // the opened scene's first frame on the board
                    }
                    if let Some(scene) = self.scenes.get(slot).cloned() {
                        let n = cycle * self.scenes.len() as u64 + slot as u64;
                        self.pass = Some((scene.clone(), n));
                        let scale = self.pass_scales.get(slot).copied().unwrap_or(0.0);
                        self.cur_scale = Some(scale);
                        if let Some(tp) = &mut self.tpipe {
                            tp.pass_start(Instant::now(), n, &scene, index, scale);
                            let j = serde_json::json!({"mark": {"pass": n, "scene": scene, "start": index}});
                            self.log_line(&j.to_string());
                        }
                    }
                }
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
            // high up: the panel scrolls, these must be visible at the booth
            ("transcription".into(), self.tstatus.as_ref().map_or("off".into(), |t| t.to_string())),
            ("text latency".into(), self.text_latency_ms.map_or("—".into(), |m| format!("{m:.0} ms"))),
            ("listening".into(), self.listen_label()),
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

/// Height of one text lane, and its fonts (readable on a 32" 4K screen).
const LANE_H: f32 = 40.0;
const LANE_LABEL_PX: f32 = 18.0;
const LANE_TEXT_PX: f32 = 21.0;
const LANE_MSG_PX: f32 = 22.0;
/// Top-bar text (title, channel buttons, source, view switch).
const TOP_PX: f32 = 20.0;

/// Left edge of a sentence box `w` wide that starts at `x0`: pulled left so
/// it ends at the pane's right edge rather than being cut off there.
fn chip_x(x0: f32, w: f32, right: f32) -> f32 {
    x0.min(right - w)
}

/// The window's zoom factor: `--ui-scale`, else `PDM_UI_SCALE`, else 1.0; 0.5 to 4.
pub fn ui_scale(flag: Option<f32>, env: Option<&str>) -> f32 {
    flag.or_else(|| env.and_then(|e| e.trim().parse().ok())).filter(|v: &f32| v.is_finite()).unwrap_or(1.0).clamp(0.5, 4.0)
}

/// Side-panel line titles: readable at booth distance (were egui `small().weak()`).
const LABEL_PX: f32 = 15.0;
const LABEL_COLOUR: Color32 = Color32::from_rgb(190, 190, 205);

/// Seconds shown across a pane: `COLUMNS` columns of hop 128 at 16 kHz.
pub const SPAN_S: f32 = (COLUMNS * 128) as f32 / 16000.0;
/// Top of the frequency axis (Nyquist of 16 kHz PCM).
const F_MAX_HZ: f32 = 8000.0;
/// Width of the axis label column left of a pane and of the colour bar right of it.
const AXIS_W: f32 = 62.0;
const BAR_W: f32 = 84.0;
/// Scale labels: as large as the side-panel titles (user).
const AXIS_PX: f32 = 15.0;
const TIME_STRIP_H: f32 = 24.0;
const AXIS_TEXT: Color32 = LABEL_COLOUR;

/// Screen y of `hz` in a pane from `top` to `bottom` (0 Hz at the bottom).
pub fn freq_y(top: f32, bottom: f32, hz: f32) -> f32 {
    bottom - (hz / F_MAX_HZ) * (bottom - top)
}

/// Screen x of a column `secs_ago` old in a pane from `left` to `right` (now on the right).
pub fn time_x(left: f32, right: f32, secs_ago: f32) -> f32 {
    right - (secs_ago / SPAN_S) * (right - left)
}

/// Colour scale of a pane: dB range of `heat`, or the ±30 dB of `diff_heat`.
#[derive(Clone, Copy)]
enum Scale {
    Level(f32, f32),
    Diff,
}

/// The image area of a pane row: label column on the left, colour bar on the right.
fn image_rect(r: Rect) -> Rect {
    Rect::from_min_max(pos2(r.left() + AXIS_W, r.top()), pos2(r.right() - BAR_W, r.bottom()))
}

/// kHz labels and faint grid lines (every 1 kHz, labels every 2 kHz).
fn freq_axis(painter: &egui::Painter, img: Rect) {
    let font = egui::FontId::proportional(AXIS_PX);
    for k in 0..=8 {
        let y = freq_y(img.top(), img.bottom(), k as f32 * 1000.0);
        let grid = Color32::from_rgba_unmultiplied(255, 255, 255, if k % 2 == 0 { 34 } else { 16 });
        painter.line_segment([pos2(img.left(), y), pos2(img.right(), y)], (1.0, grid));
        painter.line_segment([pos2(img.left() - 4.0, y), pos2(img.left(), y)], (1.0, AXIS_TEXT));
        if k % 2 == 0 {
            let align = match k { 0 => egui::Align2::RIGHT_BOTTOM, 8 => egui::Align2::RIGHT_TOP, _ => egui::Align2::RIGHT_CENTER };
            painter.text(pos2(img.left() - 6.0, y), align, format!("{k} kHz"), font.clone(), AXIS_TEXT);
        }
    }
}

/// Vertical colour bar right of the image with its end values.
fn colour_bar(painter: &egui::Painter, r: Rect, scale: Scale) {
    let bar = Rect::from_min_max(pos2(r.right() - BAR_W + 8.0, r.top() + 2.0), pos2(r.right() - BAR_W + 20.0, r.bottom() - 2.0));
    let steps = 48;
    for i in 0..steps {
        let t = (i as f32 + 0.5) / steps as f32;            // 0 bottom .. 1 top
        let c = match scale {
            Scale::Level(lo, hi) => heat(lo + t * (hi - lo), (lo, hi)),
            Scale::Diff => diff_heat(-30.0 + t * 60.0),
        };
        let y1 = bar.bottom() - i as f32 * bar.height() / steps as f32;
        let y0 = y1 - bar.height() / steps as f32;
        painter.rect_filled(Rect::from_min_max(pos2(bar.left(), y0), pos2(bar.right(), y1 + 0.5)), 0.0, c);
    }
    let font = egui::FontId::proportional(AXIS_PX);
    let (top, bottom) = match scale {
        Scale::Level(lo, hi) => (format!("{hi:.0} dB"), format!("{lo:.0}")),
        Scale::Diff => ("+30 dB".into(), "-30".into()),
    };
    painter.text(pos2(bar.right() + 3.0, bar.top()), egui::Align2::LEFT_TOP, top, font.clone(), AXIS_TEXT);
    painter.text(pos2(bar.right() + 3.0, bar.bottom()), egui::Align2::LEFT_BOTTOM, bottom, font, AXIS_TEXT);
}

/// Time strip under the panes: -10 s .. now, ticks every second.
fn time_axis(ui: &mut egui::Ui) {
    let (resp, painter) = ui.allocate_painter(vec2(ui.available_width(), TIME_STRIP_H), Sense::hover());
    let img = image_rect(resp.rect);
    let font = egui::FontId::proportional(AXIS_PX);
    for s in 0..=SPAN_S as i32 {
        let x = time_x(img.left(), img.right(), s as f32);
        painter.line_segment([pos2(x, img.top()), pos2(x, img.top() + 4.0)], (1.0, AXIS_TEXT));
        if s % 2 == 0 {
            let align = match s { 0 => egui::Align2::RIGHT_TOP, _ if s as f32 >= SPAN_S => egui::Align2::LEFT_TOP, _ => egui::Align2::CENTER_TOP };
            let text = if s == 0 { "now".to_string() } else { format!("-{s} s") };
            painter.text(pos2(x, img.top() + 4.0), align, text, font.clone(), AXIS_TEXT);
        }
    }
}

fn pane(ui: &mut egui::Ui, h: f32, tex: &Option<TextureHandle>, label: &str, empty: &str, scale: Scale) {
    let (resp, painter) = ui.allocate_painter(vec2(ui.available_width(), h), Sense::hover());
    let row = resp.rect;
    let r = image_rect(row);
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
    if let Scale::Level(..) = scale {
        freq_axis(&painter, r);
    }
    colour_bar(&painter, row, scale);
    painter.text(r.left_top() + vec2(6.0, 4.0), egui::Align2::LEFT_TOP, label,
                 egui::FontId::proportional(14.0), Color32::from_rgb(230, 230, 230));
}

impl eframe::App for App {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = ui.ctx().clone();
        self.drain();
        self.release_input();
        self.poll_text();
        if let Some(now) = self.now_index() {
            self.lanes.prune(now);
        }
        let fps = self.fps();
        self.update_textures(&ctx);
        egui::Panel::top("toolbar").show(ui, |ui| {
            ui.horizontal_wrapped(|ui| {
                let big = |t: &str| egui::RichText::new(t).size(TOP_PX);
                ui.label(big("PDM audio over Ethernet").strong());
                ui.separator();
                ui.label(big("channel"));
                for c in 0..8 {
                    if ui.selectable_label(self.sel == c, big(&format!("ch{c}"))).clicked() && self.sel != c {
                        self.sel = c;
                        self.dirty = true;
                        if let Some(s) = &self.sel_shared {
                            s.store(c, Ordering::Relaxed);
                        }
                    }
                    let on = self.listen == Listen::Channel(c);
                    if speaker_toggle(ui, on, TOP_PX * 1.3)
                        .on_hover_text(format!("hear ch{c} (board output); again = off"))
                        .clicked() {
                        self.set_listen(Listen::Channel(c));
                    }
                }
                ui.separator();
                ui.label(big(&self.source));
                if let Some(e) = &self.error {
                    ui.colored_label(Color32::RED, big(e));
                }
                // the view switch sits at the right when it fits, else it wraps onto a second row
                let views = |ui: &mut egui::Ui| {
                    ui.add_enabled(false, egui::Button::new(big("booth view"))).on_disabled_hover_text("for the real-mic project");
                    let _ = ui.selectable_label(true, big("engineering"));
                };
                if ui.max_rect().right() - ui.cursor().min.x < TOP_PX * 15.0 {
                    ui.separator();
                    let _ = ui.selectable_label(true, big("engineering"));
                    ui.add_enabled(false, egui::Button::new(big("booth view"))).on_disabled_hover_text("for the real-mic project");
                    return;
                }
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    views(ui);
                });
            });
        });
        egui::Panel::right("numbers").resizable(false).exact_size(320.0).show(ui, |ui| {
            // scrolls when the window is too low for the sources and all numbers
            egui::ScrollArea::vertical().show(ui, |ui| {
                ui.add_space(6.0);
                if self.cmds.is_some() {
                    ui.scope(|ui| self.sources_table(ui)); // its disable() stays inside
                    ui.separator();
                }
                for (k, v) in self.side_lines(fps) {
                    ui.label(egui::RichText::new(k).size(LABEL_PX).color(LABEL_COLOUR));
                    ui.add(egui::Label::new(egui::RichText::new(v).size(18.0).strong()).wrap());
                    ui.add_space(4.0);
                }
            });
        });
        egui::CentralPanel::default().show(ui, |ui| {
            let lanes_h = self.lanes_h();
            let h = ((ui.available_height() - 40.0 - TIME_STRIP_H - 26.0 - lanes_h - 4.0) / 2.0).max(60.0);
            let c = self.sel;
            let o = self.out[c].range();
            let i = self.inp.as_ref().map_or(o, |inp| inp[c].range());
            pane(ui, h, &self.tex[0], &self.in_label(c),
                 "no input stream: choose a source on the right", Scale::Level(i.0, i.1));
            pane(ui, h, &self.tex[1], &format!("ch{c} OUT (board, PDM round trip)"), "waiting for the board",
                 Scale::Level(o.0, o.1));
            pane(ui, 40.0, &self.tex[2], "difference IN - OUT", "", Scale::Diff);
            if let Some(az) = self.draw_lanes(ui, lanes_h) {
                // the same talker again = off (set_listen toggles on an equal value)
                let same = matches!(self.listen, Listen::Talker(a) if crate::lanes::ang_dist(a, az) <= crate::lanes::REUSE_DEG);
                self.set_listen(if same { self.listen } else { Listen::Talker(az) });
            }
            time_axis(ui);
        });
        self.screenshot(&ctx);
        ctx.request_repaint_after(Duration::from_millis(16));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn frequency_axis_runs_bottom_to_top() {
        assert_eq!(freq_y(10.0, 110.0, 0.0), 110.0);
        assert_eq!(freq_y(10.0, 110.0, 8000.0), 10.0);
        assert_eq!(freq_y(10.0, 110.0, 4000.0), 60.0);
    }

    #[test]
    fn time_axis_has_now_on_the_right() {
        assert_eq!(SPAN_S, 10.0);
        assert_eq!(time_x(100.0, 1100.0, 0.0), 1100.0);
        assert_eq!(time_x(100.0, 1100.0, SPAN_S), 100.0);
        assert_eq!(time_x(100.0, 1100.0, 2.5), 850.0);
    }

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
        std::thread::spawn(move || run_net(link, None, NetCtl::idle(), tx, Arc::new(|| {})));
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
        let input = NetInput { set, lead: 8000, sel: sel.clone(), export: None, marks: Vec::new() };
        std::thread::spawn(move || run_net(link, Some(input), NetCtl::idle(), tx, Arc::new(|| {})));
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

    /// A 4000-frame loop, frame i of channel c = i + c, pass starts at 0 and 1500; messages for `ms`.
    fn run_marks(cfg: crate::emulator::EmuConfig, ms: u64) -> (InputSet, Vec<(usize, u64, u64)>, std::collections::HashMap<u64, [i16; 8]>) {
        use crate::emulator::Emulator;
        let (tx, rx) = std::sync::mpsc::channel();
        let link: Box<dyn Link> = Box::new(Emulator::new(cfg));
        let mut set = InputSet::default();
        for c in 0..8 {
            set.ch[c] = (0..4000).map(|i| i + c as i16).collect();
        }
        let input = NetInput { set: set.clone(), lead: 8000, sel: Arc::new(AtomicUsize::new(0)), export: None, marks: vec![0, 1500] };
        std::thread::spawn(move || run_net(link, Some(input), NetCtl::idle(), tx, Arc::new(|| {})));
        let t0 = Instant::now();
        let (mut marks, mut ins) = (Vec::new(), std::collections::HashMap::new());
        while t0.elapsed() < Duration::from_millis(ms) {
            match rx.recv_timeout(Duration::from_millis(50)) {
                Ok(NetMsg::Mark { slot, cycle, index, .. }) => marks.push((slot, cycle, index)),
                Ok(NetMsg::In(v)) => ins.extend(v),
                _ => {}
            }
        }
        (set, marks, ins)
    }

    #[test]
    fn marks_carry_board_indices() {
        let (set, marks, ins) = run_marks(crate::emulator::EmuConfig::default(), 600);
        assert!(marks.len() >= 2, "{marks:?}");
        assert_eq!((marks[0].0, marks[1].0), (0, 1));
        assert_eq!(marks[1].2 - marks[0].2, 1500);
        assert_eq!(ins[&marks[0].2], set.frame(0));
        assert_eq!(ins[&marks[1].2], set.frame(1500));
    }

    #[test]
    fn marks_repeat_every_cycle() {
        let (_, marks, _) = run_marks(crate::emulator::EmuConfig::default(), 900);
        let first = marks[0].2;
        let again = marks.iter().find(|m| m.0 == 0 && m.1 == 1).expect("slot 0, cycle 1");
        assert_eq!(again.2, first + 4000);
    }

    #[test]
    fn board_reset_does_not_stop_marks() {
        let cfg = crate::emulator::EmuConfig { reset_after_ms: Some(400), ..Default::default() };
        let (_, marks, _) = run_marks(cfg, 2500);
        let reset = marks.windows(2).position(|w| w[1].2 < w[0].2).expect("indices restart after the reset");
        assert!(marks.len() - reset > 3, "marks stop after the reset: {marks:?}");
    }

    /// Board indices of the blocks in a shm ring, oldest first.
    fn ring_indices(name: &str) -> Vec<u64> {
        let b = std::fs::read(std::path::Path::new("/dev/shm").join(name)).unwrap();
        let n = u32::from_le_bytes(b[8..12].try_into().unwrap()) as u64;
        let w = u64::from_le_bytes(b[16..24].try_into().unwrap());
        let bb = crate::seam::BLOCK_BYTES;
        (w.saturating_sub(n)..w).map(|k| {
            let o = 64 + (k % n) as usize * bb;
            u64::from_le_bytes(b[o..o + 8].try_into().unwrap())
        }).collect()
    }

    #[test]
    fn export_has_no_gap_over_silence() {
        // 1 s of noise, then 1.5 s of silence on every channel (a playlist gap): the latency
        // correlation finds nothing over the silence, the export must go on
        use crate::emulator::{EmuConfig, Emulator};
        let (tx, rx) = std::sync::mpsc::channel();
        let link: Box<dyn Link> = Box::new(Emulator::new(EmuConfig::default()));
        let mut set = InputSet::default();
        let mut s = 7u32;
        for c in 0..8 {
            set.ch[c] = (0..40000).map(|i| if i < 16000 { s = s.wrapping_mul(1_664_525).wrapping_add(1_013_904_223); (s >> 18) as i16 } else { 0 }).collect();
        }
        let name = format!("pdm-eth-test-gap-{}", std::process::id());
        let export = crate::seam::ShmExport::create(&name, 4096).unwrap();
        let input = NetInput { set, lead: 8000, sel: Arc::new(AtomicUsize::new(0)), export: Some(export), marks: Vec::new() };
        std::thread::spawn(move || run_net(link, Some(input), NetCtl::idle(), tx, Arc::new(|| {})));
        let t0 = Instant::now();
        while t0.elapsed() < Duration::from_millis(6000) {
            let _ = rx.recv_timeout(Duration::from_millis(50));
        }
        let idx = ring_indices(&name);
        drop(rx);
        let _ = std::fs::remove_file(format!("/dev/shm/{name}")); // the test may exit before the net thread drops it
        assert!(idx.len() > 200, "{} blocks", idx.len());
        let gaps: Vec<(u64, u64)> = idx.windows(2).filter(|w| w[1] != w[0] + 64).map(|w| (w[0], w[1])).collect();
        assert!(gaps.is_empty(), "export gaps {gaps:?}");
    }

    #[test]
    fn mark_names_scene_and_pass() {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_playlist(vec!["a".into(), "b".into()]);
        tx.send(NetMsg::Mark { slot: 1, cycle: 2, index: 123, generation: 0 }).unwrap();
        app.drain();
        assert_eq!(app.pass, Some(("b".to_string(), 5)));
        assert_eq!(app.in_label(0), "ch0 IN (PC): b (pass 5)");
    }

    #[test]
    fn side_lines_show_transcription() {
        let (_tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into());
        let get = |app: &App, k: &str| app.side_lines(60.0).into_iter().find(|(a, _)| a == k).map(|(_, v)| v);
        assert_eq!(get(&app, "text latency").as_deref(), Some("—"));
        assert_eq!(get(&app, "transcription").as_deref(), Some("off"));
        app.tstatus = Some(crate::transcribe::TStatus::Running);
        app.on_text(crate::transcribe::TextLine { pass: 1, scene: "a".into(), speaker: 0, az: 30.0, start: 0, end: 16000,
                                                  text: "Hi.".into(), latency_ms: 512.4 });
        assert_eq!(get(&app, "text latency").as_deref(), Some("512 ms"));
        assert_eq!(get(&app, "transcription").as_deref(), Some("running"));
        assert_eq!(app.lanes.lanes.len(), 1);
        app.tstatus = Some(crate::transcribe::TStatus::Off("audio_pipe not found: x".into()));
        assert_eq!(get(&app, "transcription").as_deref(), Some("off: audio_pipe not found: x"));
    }

    fn wav16(path: &std::path::Path, n: usize) {
        let spec = hound::WavSpec { channels: 1, sample_rate: 16000, bits_per_sample: 16,
                                    sample_format: hound::SampleFormat::Int };
        let mut w = hound::WavWriter::create(path, spec).unwrap();
        for i in 0..n {
            w.write_sample(if i % 2 == 0 { 8000i16 } else { -8000 }).unwrap();
        }
    }

    #[test]
    fn chosen_file_goes_to_its_channel() {
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("song.wav");
        wav16(&p, 1000);
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx);
        app.choose_file(5, &p);
        match crx.try_recv().expect("a command") {
            NetCmd::SetChannel { ch, samples } => {
                assert_eq!(ch, 5);
                assert_eq!(samples.len(), 1000);
                assert_eq!(samples.iter().map(|v| v.unsigned_abs()).max(), Some(16384)); // 0.5 FS
            }
            NetCmd::SetSet { .. } => panic!("one file is not a set"),
            NetCmd::SetPlaylist { .. } => panic!("not a playlist"),
        }
        assert_eq!(app.sources[5], Source::File("song.wav".into()));
        assert!(app.error.is_none());
    }

    #[test]
    fn chip_stays_inside_the_pane() {
        // a new sentence is wider than its time span: it is pulled left, not clipped at "now"
        assert_eq!(chip_x(900.0, 300.0, 1000.0), 700.0);
        assert_eq!(chip_x(500.0, 300.0, 1000.0), 500.0);
    }

    #[test]
    fn ui_scale_flag_env_default() {
        assert_eq!(ui_scale(Some(1.75), Some("2")), 1.75);
        assert_eq!(ui_scale(None, Some("2")), 2.0);
        assert_eq!(ui_scale(None, Some("x")), 1.0);
        assert_eq!(ui_scale(None, None), 1.0);
        assert_eq!(ui_scale(Some(9.0), None), 4.0);
    }

    /// A scene folder `name` with mic1..8.wav (16 kHz, 1000 frames).
    fn scene8(dir: &std::path::Path, name: &str) -> std::path::PathBuf {
        let d = dir.join(name);
        std::fs::create_dir_all(&d).unwrap();
        for m in 1..=8 {
            wav16(&d.join(format!("mic{m}.wav")), 1000);
        }
        d
    }

    #[test]
    fn open_scene_replaces_the_playlist() {
        let d = tempfile::tempdir().unwrap();
        let p = scene8(d.path(), "myscene");
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx).with_playlist(vec!["factory_a".into(), "factory_b".into()]);
        app.lanes.add(crate::transcribe::TextLine { pass: 1, scene: "factory_a".into(), speaker: 0, az: 30.0, start: 0, end: 16000,
                                                     text: "Hi.".into(), latency_ms: 500.0 });
        app.open_scene(&p);
        match crx.try_recv().expect("a command") {
            NetCmd::SetPlaylist { set, marks, .. } => {
                assert_eq!(marks, vec![0]);
                assert_eq!(set.ch[0].len(), 1000 + crate::playlist::GAP_FRAMES);
            }
            _ => panic!("not a playlist"),
        }
        assert_eq!(app.scenes, vec!["myscene".to_string()]);
        assert_eq!(app.pass_scales.len(), 1);
        assert!(app.lanes.lanes.is_empty());
        assert!(app.error.is_none());
    }

    fn text_at(start: u64) -> crate::transcribe::TextLine {
        crate::transcribe::TextLine { pass: 0, scene: "x".into(), speaker: 0, az: 30.0, start, end: start + 16000, text: "Hi.".into(), latency_ms: 500.0 }
    }

    #[test]
    fn stale_marks_after_open_scene_are_ignored() {
        // marks of the old playlist still queued (e.g. while the file dialog was open) are not the new scene's
        let d = tempfile::tempdir().unwrap();
        let p = scene8(d.path(), "myscene");
        let (tx, rx) = std::sync::mpsc::channel();
        let (ctx, _crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx).with_playlist(vec!["factory_a".into(), "factory_b".into()]);
        app.open_scene(&p);
        tx.send(NetMsg::Mark { slot: 0, cycle: 3, index: 100, generation: 0 }).unwrap();
        app.drain();
        assert_eq!(app.pass, None, "an old-playlist mark");
        tx.send(NetMsg::Mark { slot: 0, cycle: 0, index: 500, generation: 1 }).unwrap();
        app.drain();
        assert_eq!(app.pass, Some(("myscene".to_string(), 0)));
    }

    #[test]
    fn old_scene_text_after_open_scene_is_dropped() {
        // the old pass is flushed when audio_pipe sees the new mark: its (cut) sentences must not fill the new lanes
        let d = tempfile::tempdir().unwrap();
        let p = scene8(d.path(), "myscene");
        let (tx, rx) = std::sync::mpsc::channel();
        let (ctx, _crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx).with_playlist(vec!["factory_a".into()]);
        app.open_scene(&p);
        app.on_text(text_at(50));
        assert!(app.lanes.lanes.is_empty(), "text before the new scene's first mark");
        tx.send(NetMsg::Mark { slot: 0, cycle: 0, index: 500, generation: 1 }).unwrap();
        app.drain();
        app.on_text(text_at(400));
        assert!(app.lanes.lanes.is_empty(), "old audio, before the new scene started");
        app.on_text(text_at(600));
        assert_eq!(app.lanes.lanes.len(), 1);
    }

    #[test]
    fn open_bad_scene_changes_nothing() {
        let d = tempfile::tempdir().unwrap();
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx).with_playlist(vec!["factory_a".into()]);
        app.open_scene(d.path());
        assert!(crx.try_recv().is_err());
        assert_eq!(app.scenes, vec!["factory_a".to_string()]);
        assert!(app.error.is_some());
    }

    fn line(app: &App, k: &str) -> Option<String> {
        app.side_lines(60.0).into_iter().find(|(a, _)| a == k).map(|(_, v)| v)
    }

    #[test]
    fn listen_channel_switches_and_toggles() {
        let (_tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_player_cmd(vec!["/bin/sh".into(), "-c".into(), "cat > /dev/null".into()]);
        assert_eq!(line(&app, "listening").as_deref(), Some("off"));
        app.set_listen(Listen::Channel(3));
        assert_eq!(app.listen, Listen::Channel(3));
        assert_eq!(line(&app, "listening").as_deref(), Some("ch3 board out"));
        app.set_listen(Listen::Channel(3));
        assert_eq!(app.listen, Listen::Off);
        app.set_listen(Listen::Channel(3));
        app.set_listen(Listen::Channel(5));
        assert_eq!(app.listen, Listen::Channel(5));
        assert_eq!(line(&app, "listening").as_deref(), Some("ch5 board out"));
    }

    #[test]
    fn talker_and_channel_exclude_each_other() {
        let (_tx, rx) = std::sync::mpsc::channel();
        let tp = crate::transcribe::TextPipe::new(vec!["/bin/true".into()]);
        let mut app = App::new(rx, "test".into()).with_player_cmd(vec!["/bin/sh".into(), "-c".into(), "cat > /dev/null".into()])
            .with_transcription(tp, vec![1.0], None);
        app.set_listen(Listen::Channel(2));
        assert!(app.player.is_some());
        app.set_listen(Listen::Talker(211.0));
        assert!(app.player.is_none(), "the window's channel player stops");
        assert_eq!(app.listen, Listen::Talker(211.0));
        assert_eq!(app.tpipe.as_ref().unwrap().listening(), Some(211.0));
        app.set_listen(Listen::Channel(2));
        assert_eq!(app.tpipe.as_ref().unwrap().listening(), None, "audio_pipe stops the talker");
        app.set_listen(Listen::Talker(30.0));
        app.set_listen(Listen::Talker(30.0));
        assert_eq!((app.listen, app.tpipe.as_ref().unwrap().listening()), (Listen::Off, None));
    }

    #[test]
    fn channel_backlog_is_dropped_not_played_late() {
        // after the UI thread stalled (file dialog, hidden window) the backlog would put the sound seconds behind
        let d = tempfile::tempdir().unwrap();
        let f = d.path().join("out.raw");
        let (tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_player_cmd(vec!["/bin/sh".into(), "-c".into(), format!("exec cat > {}", f.display())]);
        app.set_listen(Listen::Channel(0));
        for m in 0..100u64 {
            tx.send(NetMsg::Out((0..320).map(|k| (m * 320 + k, [256 * 100; 8])).collect())).unwrap(); // 2 s
        }
        app.drain();
        app.player.take().unwrap().close();
        let n = std::fs::metadata(&f).unwrap().len();
        assert!(n > 0 && n <= 2 * 2400, "{n} bytes: only the newest 0.15 s");
    }

    #[test]
    fn talker_without_transcription_says_unavailable() {
        let (_tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into());
        app.set_listen(Listen::Talker(30.0));
        assert_eq!(line(&app, "listening").as_deref(), Some("audio output unavailable"));
    }

    #[test]
    fn missing_player_says_unavailable() {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_player_cmd(vec!["/no/such/player".into()]);
        app.set_listen(Listen::Channel(0));
        assert_eq!(line(&app, "listening").as_deref(), Some("audio output unavailable"));
        tx.send(NetMsg::Out((0..64).map(|k| (k, [1000; 8])).collect())).unwrap();
        app.drain(); // no panic, nothing else changes
        assert!(app.out_newest.is_some());
    }

    #[test]
    fn channel_audio_goes_to_the_player() {
        let d = tempfile::tempdir().unwrap();
        let f = d.path().join("out.raw");
        let (tx, rx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_player_cmd(vec!["/bin/sh".into(), "-c".into(), format!("cat > {}", f.display())]);
        app.set_listen(Listen::Channel(2));
        let mut fr = [0i32; 8];
        fr[2] = 256 * 1000; // 24-bit board output; no playlist: >> 8
        tx.send(NetMsg::Out((0..64).map(|k| (k, fr)).collect())).unwrap();
        app.drain();
        app.set_listen(Listen::Off); // the player finishes in the background
        let t0 = Instant::now();
        while std::fs::metadata(&f).map_or(0, |m| m.len()) < 128 && t0.elapsed() < Duration::from_secs(2) {
            std::thread::sleep(Duration::from_millis(5));
        }
        let b = std::fs::read(&f).unwrap();
        assert_eq!(b.len(), 128);
        assert_eq!(i16::from_le_bytes([b[0], b[1]]), 1000);
    }

    #[test]
    fn playlist_locks_the_sources() {
        // a visitor click must not mix another signal into a scene that is being transcribed
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("song.wav");
        wav16(&p, 1000);
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx).with_playlist(vec!["factory_a".into()]);
        let before = app.sources.clone();
        app.choose_file(2, &p);
        app.set_source(3, BUILTIN[0].clone());
        app.choose_set(vec![p.clone()]);
        assert!(crx.try_recv().is_err(), "no source command while a playlist runs");
        assert_eq!(app.sources, before);
    }

    #[test]
    fn chosen_bad_file_shows_an_error_and_changes_nothing() {
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("notes.txt");
        std::fs::write(&p, b"not audio").unwrap();
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx);
        app.choose_file(2, &p);
        assert!(crx.try_recv().is_err());
        assert!(app.error.as_deref().is_some_and(|e| e.contains("notes.txt")), "{:?}", app.error);
        assert!(app.sources.iter().all(|s| *s == Source::Booth));
    }

    #[test]
    fn chosen_builtin_source_goes_to_its_channel() {
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx);
        app.set_source(4, Source::Music);
        match crx.try_recv().expect("a command") {
            NetCmd::SetChannel { ch, samples } => assert_eq!((ch, samples.len()), (4, 480_000)),
            NetCmd::SetSet { .. } => panic!("one source is not a set"),
            NetCmd::SetPlaylist { .. } => panic!("not a playlist"),
        }
        assert_eq!(app.sources[4], Source::Music);
        app.set_source(4, Source::Booth);
        match crx.try_recv().expect("a command") {
            NetCmd::SetChannel { ch, samples } => assert_eq!(samples, crate::wavin::booth_clips()[ch]),
            NetCmd::SetSet { .. } => panic!("one source is not a set"),
            NetCmd::SetPlaylist { .. } => panic!("not a playlist"),
        }
    }

    #[test]
    fn set_channel_starts_a_stream_with_booth_clips_elsewhere() {
        use crate::emulator::{EmuConfig, Emulator};
        let (tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let link: Box<dyn Link> = Box::new(Emulator::new(EmuConfig::default()));
        let ctl = NetCtl { cmds: crx, lead: 8000, sel: Arc::new(AtomicUsize::new(0)) };
        std::thread::spawn(move || run_net(link, None, ctl, tx, Arc::new(|| {})));
        std::thread::sleep(Duration::from_millis(200));
        ctx.send(NetCmd::SetChannel { ch: 2, samples: vec![1234; 640] }).unwrap();
        std::thread::sleep(Duration::from_millis(600));
        let mut sent = Vec::new();
        while let Ok(m) = rx.try_recv() {
            if let NetMsg::In(v) = m {
                sent.extend(v);
            }
        }
        assert!(sent.len() > 1000, "only {} input frames", sent.len());
        assert!(sent.iter().all(|(_, f)| f[2] == 1234), "ch2 plays the chosen samples");
        let booth = crate::wavin::booth_clips();
        assert!(sent.iter().enumerate().all(|(k, (_, f))| f[3] == booth[3][k % 16384]), "ch3 plays its booth clip");
    }

    #[test]
    fn capture_set_starts_all_channels_together_at_their_first_sample() {
        use crate::emulator::{EmuConfig, Emulator};
        let (tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let link: Box<dyn Link> = Box::new(Emulator::new(EmuConfig::default()));
        let ctl = NetCtl { cmds: crx, lead: 8000, sel: Arc::new(AtomicUsize::new(0)) };
        std::thread::spawn(move || run_net(link, None, ctl, tx, Arc::new(|| {})));
        std::thread::sleep(Duration::from_millis(200));
        ctx.send(NetCmd::SetChannel { ch: 7, samples: vec![5; 64] }).unwrap(); // stream runs
        std::thread::sleep(Duration::from_millis(300));
        let ramp: Vec<i16> = (0..2000).collect();
        // ch1 below -16384: no booth clip goes there, so the set is found unambiguously
        ctx.send(NetCmd::SetSet { chans: vec![(0, ramp.clone()), (1, ramp.iter().map(|v| v - 20000).collect())] }).unwrap();
        std::thread::sleep(Duration::from_millis(500));
        let mut sent = Vec::new();
        while let Ok(m) = rx.try_recv() {
            if let NetMsg::In(v) = m {
                sent.extend(v);
            }
        }
        let k = sent.iter().position(|(_, f)| f[1] < -16384).expect("the set was sent");
        assert_eq!(sent[k].1[0], 0, "the set starts at its first sample");
        assert!(sent[k..].iter().all(|(_, f)| f[1] == f[0] - 20000), "ch0 and ch1 stay sample-aligned");
        assert!(sent.len() - k > 1000, "only {} frames after the set", sent.len() - k);
    }

    #[test]
    fn chosen_capture_set_goes_to_ch0_up_in_name_order() {
        let d = tempfile::tempdir().unwrap();
        let (a, b) = (d.path().join("mic_a.wav"), d.path().join("mic_b.wav"));
        wav16(&a, 900);
        wav16(&b, 1000);
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx);
        app.choose_set(vec![b, a]); // the dialog's order does not matter
        match crx.try_recv().expect("a command") {
            NetCmd::SetSet { chans } => {
                let v: Vec<(usize, usize)> = chans.iter().map(|(c, x)| (*c, x.len())).collect();
                assert_eq!(v, vec![(0, 900), (1, 900)], "trimmed to the shortest");
            }
            NetCmd::SetChannel { .. } => panic!("a set goes as one command"),
            NetCmd::SetPlaylist { .. } => panic!("a set is not a playlist"),
        }
        assert_eq!(app.sources[0].label(), "mic_a.wav (set)");
        assert_eq!(app.sources[1].label(), "mic_b.wav (set)");
        assert_eq!(app.sources[2], Source::Booth);
        let info = app.set_info.clone().expect("set info");
        assert!(info.contains("2 ch") && info.contains("100"), "{info}");
    }

    #[test]
    fn capture_set_that_does_not_load_changes_nothing() {
        let d = tempfile::tempdir().unwrap();
        let a = d.path().join("a.wav");
        wav16(&a, 900);
        let (_tx, rx) = std::sync::mpsc::channel();
        let (ctx, crx) = std::sync::mpsc::channel();
        let mut app = App::new(rx, "test".into()).with_commands(ctx);
        app.choose_set(vec![a.clone(), d.path().join("missing.wav")]);
        assert!(crx.try_recv().is_err());
        assert!(app.error.as_deref().is_some_and(|e| e.contains("missing.wav")), "{:?}", app.error);
        assert!(app.sources.iter().all(|s| *s == Source::Booth));
    }
}
