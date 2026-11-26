// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! `audio_pipe live`: the pipeline on the board output that `pdm-eth gui
//! --export-shm` writes to `/dev/shm`. The GUI sends one control line per
//! scene pass on stdin; every pass is a fresh pipeline run, and each
//! accepted final utterance goes out as one JSON line on stdout.

use crate::asr::{AsrResult, AsrWorker};
use crate::common::{FS, H, M};
use crate::pipeline::Pipeline;
use crate::shm::{Block, Read, ShmReader};
use crate::PipeArgs;
use anyhow::Result;
use std::collections::VecDeque;
use std::io::Write;
use std::time::{Duration, Instant};

#[derive(clap::Args)]
pub struct LiveArgs {
    /// ring name in /dev/shm (pdm-eth gui --export-shm NAME)
    #[arg(long)]
    pub shm: String,
    #[command(flatten)]
    pub p: PipeArgs,
}

/// Control line from the GUI: a scene pass starts at board index `start`;
/// output samples are `out_i32 * scale` (the file level).
#[derive(Clone, Debug, PartialEq)]
pub struct PassStart {
    pub pass: u64,
    pub scene: String,
    pub start: u64,
    pub scale: f32,
}

pub fn handle_control(line: &str) -> Option<PassStart> {
    let v: serde_json::Value = serde_json::from_str(line.trim()).ok()?;
    let scale = v.get("scale")?.as_f64()? as f32;
    if !(scale.is_finite() && scale > 0.0) {
        return None;
    }
    Some(PassStart { pass: v.get("pass")?.as_u64()?, scene: v.get("scene")?.as_str()?.to_string(), start: v.get("start")?.as_u64()?, scale })
}

fn to_index(pass: &PassStart, t: f64) -> u64 {
    pass.start + (t * FS as f64).round().max(0.0) as u64
}

pub fn text_line(pass: &PassStart, r: &AsrResult, az: f32, latency_ms: f32) -> String {
    serde_json::json!({
        "pass": pass.pass, "scene": pass.scene, "speaker": r.speaker, "az": (az as f64 * 10.0).round() / 10.0,
        "start": to_index(pass, r.t_start), "end": to_index(pass, r.t_speech_end),
        "text": r.text, "latency_ms": latency_ms.round(),
    })
    .to_string()
}

/// Lost blocks up to this many are filled with zeros; a larger jump drops the pass.
const MAX_FILL: u64 = 8;
/// Longest wait for the last utterances of a pass at its end.
const FLUSH_WAIT: Duration = Duration::from_secs(3);
/// A pass start further ahead of the reader than this (10 s) is stale (from before a board reset).
const MAX_AHEAD: u64 = 10 * FS as u64;

/// `{"listen_az": deg}` / `{"listen_az": null}` from the GUI: which talker to play.
pub fn handle_listen(line: &str) -> Option<Option<f32>> {
    let v: serde_json::Value = serde_json::from_str(line.trim()).ok()?;
    match v.as_object()?.get("listen_az")? {
        serde_json::Value::Null => Some(None),
        x => x.as_f64().map(|a| Some(a as f32)),
    }
}

/// The stream nearest `az` (circular distance at most 20°, the GUI's lane rule).
pub fn choose(streams: &[(usize, f32)], az: f32) -> Option<usize> {
    let d = |a: f32| {
        let x = (a - az).rem_euclid(360.0);
        x.min(360.0 - x)
    };
    streams.iter().filter(|(_, a)| d(*a) <= 20.0).min_by(|x, y| d(x.1).total_cmp(&d(y.1))).map(|(id, _)| *id)
}

/// Read position in one stream's separated output (`pcm` from input sample `origin`).
pub struct Tap {
    pub id: usize,
    pub upto: i64,
}

impl Tap {
    /// Starts `TAP_LOOKBACK` before the stream's current end (not earlier than
    /// its first sample): a stream is born 0.1-0.2 s after its talker started,
    /// so this keeps the first syllable without replaying all its history.
    pub fn new(id: usize, origin: i64, pcm: &[f32]) -> Self {
        Tap { id, upto: (origin + pcm.len() as i64 - TAP_LOOKBACK).max(origin) }
    }

    /// The samples after the last take.
    pub fn take<'a>(&mut self, origin: i64, pcm: &'a [f32]) -> &'a [f32] {
        let a = (self.upto - origin).clamp(0, pcm.len() as i64) as usize;
        self.upto = origin + pcm.len() as i64;
        &pcm[a..]
    }
}

/// History played when a talker's stream is chosen (0.3 s).
const TAP_LOOKBACK: i64 = 4800;

/// Talker audio gain to the player (the separated stream is quiet), then a
/// soft limit at 0.95 FS (tanh) instead of clipping.
const LISTEN_GAIN: f32 = 4.0;

fn listen_sample(v: f32) -> i16 {
    ((v * LISTEN_GAIN).tanh() * 0.95 * 32767.0) as i16
}

pub struct Live {
    p: PipeArgs,
    active: [bool; M],
    pub pipe: Option<Pipeline>,
    /// the ASR worker while no pipeline runs (moved between pipelines, the model stays loaded)
    asr: Option<AsrWorker>,
    pending: VecDeque<PassStart>,
    cur: Option<PassStart>,
    hop: Vec<Vec<f32>>,
    fill: usize,
    /// board index of the block expected next
    next: Option<u64>,
    /// results received from the current pipeline
    received: usize,
    /// results still to come from dropped or timed-out pipelines (discarded)
    discard: usize,
    /// (frame, when it was processed), last 10 s
    times: VecDeque<(usize, Instant)>,
    status: String,
    /// JSON lines for stdout
    pub out: Vec<String>,
    /// The whisper thread has stopped (its result channel is closed).
    asr_dead: bool,
    /// Talker listening: azimuth asked for by the GUI, the chosen stream, the player.
    listen_az: Option<f32>,
    tap: Option<Tap>,
    player: Option<crate::player::Player>,
    player_cmd: Vec<String>,
}

impl Live {
    pub fn new(a: LiveArgs, asr: Option<AsrWorker>) -> Result<Self> {
        let mut p = a.p;
        p.partial_ms = 0; // final text only
        let active = crate::mics_active(&p)?;
        let mut l = Live {
            p,
            active,
            pipe: None,
            asr,
            pending: VecDeque::new(),
            cur: None,
            hop: vec![vec![0.0; H]; M],
            fill: 0,
            next: None,
            received: 0,
            discard: 0,
            times: VecDeque::new(),
            status: String::new(),
            out: Vec::new(),
            asr_dead: false,
            listen_az: None,
            tap: None,
            player: None,
            player_cmd: crate::player::player_cmd(),
        };
        l.set_status("waiting", "no pass started yet");
        Ok(l)
    }

    fn set_status(&mut self, s: &str, detail: &str) {
        if self.status != s {
            self.status = s.to_string();
            self.out.push(serde_json::json!({"status": s, "detail": detail}).to_string());
        }
    }

    /// The GUI's talker choice: play the stream nearest `az`, or stop.
    pub fn listen(&mut self, az: Option<f32>) {
        self.listen_az = az;
        self.tap = None;
        if az.is_none() {
            self.player = None;
        } else if self.player.as_ref().is_none_or(|p| !p.alive()) {
            self.player = match crate::player::Player::spawn(&self.player_cmd) {
                Ok(p) => {
                    self.out.push(serde_json::json!({"listen": "ok"}).to_string());
                    Some(p)
                }
                Err(e) => {
                    let d = format!("{}: {e}", self.player_cmd.join(" "));
                    eprintln!("live: audio output unavailable ({d})");
                    self.out.push(serde_json::json!({"listen": "unavailable", "detail": d}).to_string());
                    None
                }
            };
        } else {
            self.out.push(serde_json::json!({"listen": "ok"}).to_string());
        }
    }

    /// A talker player that has died is reported and dropped (the GUI's next choice starts a new one).
    pub fn check_player(&mut self) {
        if self.player.as_ref().is_some_and(|p| !p.alive()) {
            self.player = None;
            eprintln!("live: audio output ended");
            self.out.push(serde_json::json!({"listen": "unavailable", "detail": "the player exited"}).to_string());
        }
    }

    /// After a hop: new samples of the chosen talker's stream to the player.
    fn feed_listen(&mut self) {
        let (Some(az), Some(pipe), Some(player)) = (self.listen_az, &self.pipe, &self.player) else { return };
        if self.tap.as_ref().is_none_or(|t| !pipe.streams.iter().any(|s| s.id == t.id)) {
            let st: Vec<(usize, f32)> = pipe.streams.iter().map(|s| (s.id, s.az)).collect();
            self.tap = choose(&st, az).and_then(|id| pipe.streams.iter().find(|s| s.id == id)).map(|s| Tap::new(s.id, s.origin, &s.pcm));
            return;
        }
        let t = self.tap.as_mut().unwrap();
        if let Some(s) = pipe.streams.iter().find(|s| s.id == t.id) {
            let x: Vec<i16> = t.take(s.origin, &s.pcm).iter().map(|&v| listen_sample(v)).collect();
            if !x.is_empty() {
                player.push(&x);
            }
        }
    }

    /// A pass start from the GUI. One the reader has already passed is skipped.
    pub fn control(&mut self, ps: PassStart) {
        if let Some(n) = self.next {
            if ps.start < n {
                eprintln!("live: pass {} start {} is behind the reader ({n}), skipped", ps.pass, ps.start);
                return;
            }
            if ps.start > n + MAX_AHEAD {
                eprintln!("live: pass {} start {} is far ahead of the reader ({n}), skipped", ps.pass, ps.start);
                return;
            }
        }
        let i = self.pending.iter().position(|q| q.start > ps.start).unwrap_or(self.pending.len());
        self.pending.insert(i, ps);
    }

    pub fn on_read(&mut self, r: Read) -> Result<()> {
        match r {
            Read::Block(b) => {
                if let Some(n) = self.next {
                    if b.index != n {
                        let lost = b.index.wrapping_sub(n);
                        if b.index > n && lost % 64 == 0 && lost / 64 <= MAX_FILL {
                            for k in 0..lost / 64 {
                                self.block(&Block { index: n + 64 * k, out: [[0; 8]; 64] })?;
                            }
                        } else {
                            self.drop_pass(&format!("board index jumped from {n} to {}", b.index));
                            if b.index < n {
                                self.pending.clear(); // board reset: queued starts belong to the old index range
                            } else {
                                self.pending.retain(|q| q.start >= b.index);
                            }
                        }
                    }
                }
                self.block(&b)?;
            }
            Read::Empty => {}
            Read::Overrun { skipped } => {
                self.drop_pass(&format!("reader fell behind, {skipped} blocks lost"));
                self.next = None;
            }
            Read::Reattached => {
                self.drop_pass("pdm-eth restarted");
                self.pending.clear();
                self.next = None;
            }
        }
        self.poll();
        self.check()
    }

    /// An error once the whisper thread has stopped: the process must exit so the GUI restarts it.
    pub fn check(&self) -> Result<()> {
        if self.asr_dead {
            anyhow::bail!("the whisper thread stopped");
        }
        Ok(())
    }

    fn block(&mut self, b: &Block) -> Result<()> {
        self.next = Some(b.index + 64);
        for (fr, o) in b.out.iter().enumerate() {
            let idx = b.index + fr as u64;
            while self.pending.front().is_some_and(|q| q.start <= idx) {
                let q = self.pending.pop_front().unwrap();
                if q.start == idx {
                    self.begin_pass(q)?;
                } else {
                    eprintln!("live: pass {} start {} missed, skipped", q.pass, q.start);
                }
            }
            let Some(cur) = &self.cur else { continue };
            for m in 0..M {
                self.hop[m][self.fill] = o[m] as f32 * cur.scale;
            }
            self.fill += 1;
            if self.fill == H {
                self.fill = 0;
                let pipe = self.pipe.as_mut().unwrap();
                self.times.push_back((pipe.t, Instant::now()));
                while self.times.len() > 10 * FS / H {
                    self.times.pop_front();
                }
                let hops: Vec<&[f32]> = self.hop.iter().map(|v| v.as_slice()).collect();
                pipe.process(&hops);
                self.check_player();
                self.feed_listen();
            }
        }
        Ok(())
    }

    fn begin_pass(&mut self, q: PassStart) -> Result<()> {
        self.end_pass(true);
        let pipe = crate::build_pipeline(&self.p, &self.active, self.asr.take())?;
        eprintln!("live: pass {} ({}) from board index {}", q.pass, q.scene, q.start);
        self.pipe = Some(pipe);
        self.tap = None; // stream ids are new each pass
        self.cur = Some(q);
        self.fill = 0;
        self.set_status("running", "");
        Ok(())
    }

    /// Ends the current pass: with `flush`, closes its utterances and emits their text (waits at most 3 s).
    fn end_pass(&mut self, flush: bool) {
        let Some(mut pipe) = self.pipe.take() else {
            self.cur = None;
            return;
        };
        if flush && pipe.asr.is_some() {
            pipe.flush();
            let t0 = Instant::now();
            self.pipe = Some(pipe);
            while self.received < self.pipe.as_ref().unwrap().jobs_sent && t0.elapsed() < FLUSH_WAIT {
                if !self.poll() {
                    std::thread::sleep(Duration::from_millis(2));
                }
            }
            pipe = self.pipe.take().unwrap();
        }
        if let Some(c) = &self.cur {
            eprintln!("live: pass {} ended{}: {} frames, {} streams, {} of {} results", c.pass, if flush { "" } else { " (dropped)" },
                      pipe.t, pipe.streams.len(), self.received, pipe.jobs_sent);
        }
        self.discard += pipe.jobs_sent.saturating_sub(self.received);
        self.asr = pipe.asr.take();
        if self.asr.is_none() {
            self.discard = 0;
        }
        self.received = 0;
        self.cur = None;
        self.times.clear();
    }

    fn drop_pass(&mut self, reason: &str) {
        if self.pipe.is_some() || self.cur.is_some() {
            eprintln!("live: pass dropped: {reason}");
            self.end_pass(false);
            self.set_status("overrun", reason);
        }
    }

    /// Takes the ASR results that have arrived; true if there were any.
    pub fn poll(&mut self) -> bool {
        let rx = match (&self.pipe, &self.asr) {
            (Some(p), _) => p.asr.as_ref().map(|w| w.rx.clone()),
            (None, Some(w)) => Some(w.rx.clone()),
            _ => None,
        };
        let Some(rx) = rx else { return false };
        let mut any = false;
        loop {
            let r = match rx.try_recv() {
                Ok(r) => r,
                Err(crossbeam_channel::TryRecvError::Empty) => break,
                Err(crossbeam_channel::TryRecvError::Disconnected) => {
                    self.asr_dead = true;
                    break;
                }
            };
            any = true;
            if self.discard > 0 {
                self.discard -= 1;
                continue;
            }
            let (Some(pipe), Some(cur)) = (&self.pipe, &self.cur) else { continue };
            self.received += 1;
            if r.partial || !r.hyp.accepted || r.text.trim().is_empty() {
                continue;
            }
            let az = pipe.streams.iter().find(|s| s.id == r.speaker).map_or(f32::NAN, |s| s.az);
            let end_frame = ((r.t_speech_end * FS as f64 / H as f64).round() as usize).saturating_sub(1);
            let lat = match self.times.iter().find(|(t, _)| *t == end_frame) {
                Some((_, at)) => r.done.saturating_duration_since(*at),
                None => r.done.saturating_duration_since(r.submitted),
            };
            self.out.push(text_line(cur, &r, az, lat.as_secs_f32() * 1e3));
        }
        any
    }
}

/// `audio_pipe live --shm NAME`: runs until stdin closes (the GUI exited).
pub fn run(a: LiveArgs) -> Result<()> {
    let path = std::path::Path::new("/dev/shm").join(&a.shm);
    let asr = crate::spawn_asr(&a.p)?;
    let mut live = Live::new(a, asr)?;
    let (tx, rx) = crossbeam_channel::unbounded::<String>();
    std::thread::spawn(move || {
        let stdin = std::io::stdin();
        let mut line = String::new();
        while stdin.read_line(&mut line).map(|n| n > 0).unwrap_or(false) {
            if tx.send(std::mem::take(&mut line)).is_err() {
                return;
            }
        }
    });
    let mut reader: Option<ShmReader> = None;
    let mut stdout = std::io::stdout().lock();
    let mut waited = false;
    loop {
        loop {
            match rx.try_recv() {
                Ok(l) => match (handle_listen(&l), handle_control(&l)) {
                    (Some(az), _) => live.listen(az),
                    (None, Some(ps)) => live.control(ps),
                    (None, None) => eprintln!("live: control line ignored: {}", l.trim()),
                },
                Err(crossbeam_channel::TryRecvError::Empty) => break,
                Err(crossbeam_channel::TryRecvError::Disconnected) => {
                    live.end_pass(true);
                    for l in live.out.drain(..) {
                        writeln!(stdout, "{l}")?;
                    }
                    stdout.flush()?;
                    // join the whisper thread before exit: the CUDA context must not be torn down under it
                    if let Some(w) = live.asr.take() {
                        w.finish();
                    }
                    return Ok(());
                }
            }
        }
        let idle = match &mut reader {
            None => match ShmReader::open(&path) {
                Ok(r) => {
                    eprintln!("live: reading {}", path.display());
                    reader = Some(r);
                    false
                }
                Err(e) => {
                    if !waited {
                        eprintln!("live: waiting for {}: {e}", path.display());
                        waited = true;
                    }
                    std::thread::sleep(Duration::from_millis(50));
                    true
                }
            },
            Some(r) => match r.next() {
                Ok(Read::Empty) => {
                    live.poll();
                    live.check()?;
                    true
                }
                Ok(x) => {
                    live.on_read(x)?;
                    false
                }
                Err(e) => {
                    eprintln!("live: {}: {e}", path.display());
                    reader = None;
                    live.on_read(Read::Reattached)?;
                    true
                }
            },
        };
        if !live.out.is_empty() {
            for l in live.out.drain(..) {
                writeln!(stdout, "{l}")?;
            }
            stdout.flush()?;
        }
        if idle {
            std::thread::sleep(Duration::from_millis(2));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::shm::{Block, Read};
    use clap::Parser;

    fn args(extra: &[&str]) -> LiveArgs {
        #[derive(Parser)]
        struct T {
            #[command(flatten)]
            l: LiveArgs,
        }
        let mut v = vec!["t", "--shm", "test"];
        v.extend_from_slice(extra);
        T::parse_from(v).l
    }

    fn zero_block(index: u64) -> Read {
        Read::Block(Block { index, out: [[0; 8]; 64] })
    }

    fn ps(pass: u64, start: u64) -> PassStart {
        PassStart { pass, scene: "s".into(), start, scale: 1.0 }
    }

    #[test]
    fn control_line_parses() {
        assert_eq!(handle_control(r#"{"pass":3,"scene":"a","start":1000,"scale":0.5}"#), Some(PassStart { pass: 3, scene: "a".into(), start: 1000, scale: 0.5 }));
        assert_eq!(handle_control("garbage"), None);
    }

    #[test]
    fn text_line_has_board_indices() {
        let r = crate::asr::AsrResult::test_final(2, 0.5, 2.0, "Hi there.");
        let v: serde_json::Value = serde_json::from_str(&text_line(&PassStart { pass: 7, scene: "a".into(), start: 1_000_000, scale: 1.0 }, &r, 30.1, 412.0)).unwrap();
        assert_eq!(v["az"], 30.1);
        assert_eq!(v["start"], 1_008_000);
        assert_eq!(v["end"], 1_032_000);
        assert_eq!(v["pass"], 7);
        assert_eq!(v["speaker"], 2);
        assert_eq!(v["text"], "Hi there.");
    }

    #[test]
    fn no_output_before_first_pass_start() {
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        for b in 0..(48000 / 64) {
            l.on_read(zero_block(b * 64)).unwrap();
        }
        assert!(l.pipe.is_none(), "no pipeline before a pass start");
        l.control(ps(1, 48000));
        for b in (48000 / 64)..(48000 / 64 + 100) {
            l.on_read(zero_block(b * 64)).unwrap();
        }
        assert_eq!(l.pipe.as_ref().unwrap().t, 100 * 64 / 128, "only samples from 48000 on are processed");
        assert!(l.out.iter().all(|s| !s.contains("\"text\"")));
    }

    #[test]
    fn start_behind_the_reader_is_skipped() {
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        for b in 0..100 {
            l.on_read(zero_block(b * 64)).unwrap();
        }
        l.control(ps(1, 64)); // already read
        l.on_read(zero_block(100 * 64)).unwrap();
        assert!(l.pipe.is_none());
    }

    /// One fake utterance as if the pipeline had dispatched it.
    fn fake_job(l: &mut Live, speaker: usize) {
        let pipe = l.pipe.as_mut().unwrap();
        pipe.asr.as_ref().unwrap().tx.send(crate::asr::Job::test_final(speaker, 0.5, 1.5)).unwrap();
        pipe.jobs_sent += 1;
    }

    #[test]
    fn overrun_drops_the_pass() {
        let mut l = Live::new(args(&["--no-asr"]), Some(crate::asr::AsrWorker::echo())).unwrap();
        l.control(ps(1, 0));
        l.on_read(zero_block(0)).unwrap();
        fake_job(&mut l, 1);
        l.on_read(Read::Overrun { skipped: 3 }).unwrap();
        l.control(ps(2, 6400));
        l.on_read(zero_block(6400)).unwrap();
        fake_job(&mut l, 2);
        l.control(ps(3, 6400 + 64 * 4));
        for k in 1..=4 {
            l.on_read(zero_block(6400 + 64 * k)).unwrap();
        }
        let texts: Vec<serde_json::Value> = l.out.iter().map(|s| serde_json::from_str::<serde_json::Value>(s).unwrap()).filter(|v| v.get("text").is_some()).collect();
        assert_eq!(texts.len(), 1, "{:?}", l.out);
        assert_eq!(texts[0]["pass"], 2);
        assert_eq!(texts[0]["speaker"], 2);
        assert!(l.out.iter().any(|s| s.contains("\"overrun\"")));
    }

    #[test]
    fn index_jump_drops_the_pass() {
        let mut l = Live::new(args(&["--no-asr"]), Some(crate::asr::AsrWorker::echo())).unwrap();
        l.control(ps(1, 0));
        l.on_read(zero_block(0)).unwrap();
        l.on_read(zero_block(64 * 3)).unwrap(); // 2 lost blocks: filled with zeros
        assert!(l.pipe.is_some());
        assert_eq!(l.pipe.as_ref().unwrap().t, 2);
        fake_job(&mut l, 1);
        l.on_read(zero_block(64 * 100)).unwrap(); // large jump (board reset)
        assert!(l.pipe.is_none());
        l.control(ps(2, 64 * 200));
        l.on_read(zero_block(64 * 200)).unwrap();
        l.control(ps(3, 64 * 201));
        l.on_read(zero_block(64 * 201)).unwrap();
        assert!(l.out.iter().all(|s| !s.contains("\"text\"")), "{:?}", l.out);
    }

    #[test]
    fn stale_start_after_board_reset_is_dropped() {
        // a start queued before a board reset must not fire when the new index reaches it
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        l.control(ps(1, 64 * 500));
        l.on_read(zero_block(64 * 500)).unwrap();
        l.control(ps(2, 64 * 600)); // queued, its block never comes
        l.on_read(zero_block(64 * 10)).unwrap(); // board reset: index goes back
        assert!(l.pipe.is_none());
        for b in 11..=610 {
            l.on_read(zero_block(64 * b)).unwrap();
        }
        assert!(l.pipe.is_none(), "the stale pass 2 start must not begin a pass");
    }

    #[test]
    fn start_far_ahead_of_the_reader_is_ignored() {
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        l.on_read(zero_block(0)).unwrap();
        l.control(ps(1, 64 + 11 * FS as u64)); // more than 10 s ahead: stale
        assert!(l.pending.is_empty());
        l.control(ps(2, 64 + 2 * FS as u64));
        assert_eq!(l.pending.len(), 1);
    }

    #[test]
    fn dead_whisper_thread_is_an_error() {
        // the process must exit so the GUI restarts it, not sit at "running" without text
        let mut l = Live::new(args(&["--no-asr"]), Some(crate::asr::AsrWorker::dead())).unwrap();
        l.control(ps(1, 0));
        assert!(l.on_read(zero_block(0)).is_err());
    }

    #[test]
    fn listen_line_parses() {
        assert_eq!(handle_listen(r#"{"listen_az":211.5}"#), Some(Some(211.5)));
        assert_eq!(handle_listen(r#"{"listen_az":null}"#), Some(None));
        assert_eq!(handle_listen(r#"{"pass":1,"scene":"a","start":0,"scale":1.0}"#), None);
        assert_eq!(handle_listen("garbage"), None);
    }

    #[test]
    fn choose_nearest_within_20_deg() {
        let st = [(1, 30.0), (2, 212.0), (3, 355.0)];
        assert_eq!(choose(&st, 5.0), Some(3));
        assert_eq!(choose(&st, 211.0), Some(2));
        assert_eq!(choose(&st, 120.0), None);
    }

    #[test]
    fn tap_plays_the_last_300_ms_of_history_then_new() {
        // a stream is born 0.1-0.2 s after its talker started: 0.3 s of its history keeps the first syllable
        let pcm: Vec<f32> = (0..10_000).map(|i| i as f32).collect();
        let mut t = Tap::new(7, 0, &pcm[..8000]);
        assert_eq!(t.upto, 8000 - 4800);
        assert_eq!(t.take(0, &pcm[..8000]).len(), 4800);
        let x = t.take(0, &pcm);
        assert_eq!(x.len(), 2000);
        assert_eq!(x[0], 8000.0);
        assert!(t.take(0, &pcm).is_empty());
        // a stream younger than 0.3 s: from its first sample
        let t = Tap::new(8, 500, &pcm[..1000]);
        assert_eq!(t.upto, 500);
    }

    #[test]
    fn listen_survives_pass_change() {
        let d = tempfile::tempdir().unwrap();
        let f = d.path().join("out.raw");
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        l.player_cmd = vec!["/bin/sh".into(), "-c".into(), format!("cat > {}", f.display())];
        l.listen(Some(30.0));
        l.control(ps(1, 0));
        for b in 0..200 {
            l.on_read(zero_block(b * 64)).unwrap();
        }
        l.control(ps(2, 200 * 64));
        l.on_read(zero_block(200 * 64)).unwrap();
        assert!(l.tap.is_none(), "a new pass chooses its stream again");
        for b in 201..300 {
            l.on_read(zero_block(b * 64)).unwrap();
        }
        l.listen(None);
        assert!(l.player.is_none());
    }

    #[test]
    fn listen_plays_the_chosen_talker_on_a_scene() {
        let Ok(root) = std::env::var("SCENE_ROOT") else {
            eprintln!("SCENE_ROOT unset: listen_plays_the_chosen_talker_on_a_scene skipped");
            return;
        };
        let chans = crate::common::read_mics(&std::path::Path::new(&root).join("audio_wav_factory_4workers_9s5_b270_f150")).unwrap();
        let d = tempfile::tempdir().unwrap();
        let f = d.path().join("out.raw");
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        l.player_cmd = vec!["/bin/sh".into(), "-c".into(), format!("exec cat > {}", f.display())];
        l.listen(Some(30.0)); // the male talker
        l.control(PassStart { pass: 1, scene: "s".into(), start: 0, scale: 1.0 / (256.0 * 16384.0) });
        for b in 0..(chans[0].len() / 64) as u64 {
            let mut out = [[0i32; 8]; 64];
            for (fr, row) in out.iter_mut().enumerate() {
                for c in 0..8 {
                    row[c] = (chans[c][b as usize * 64 + fr] * 256.0 * 16384.0).round() as i32;
                }
            }
            l.on_read(Read::Block(Block { index: b * 64, out })).unwrap();
        }
        l.listen(None); // the player finishes in the background: wait until the file stops growing
        let mut last = u64::MAX;
        for _ in 0..100 {
            std::thread::sleep(std::time::Duration::from_millis(20));
            let n = std::fs::metadata(&f).map_or(0, |m| m.len());
            if n == last {
                break;
            }
            last = n;
        }
        if let Ok(keep) = std::env::var("LISTEN_KEEP") {
            let _ = std::fs::copy(&f, keep);
        }
        let b = std::fs::read(&f).unwrap();
        let x: Vec<f32> = b.chunks(2).map(|c| i16::from_le_bytes([c[0], c[1]]) as f32 / 32768.0).collect();
        let rms = (x.iter().map(|v| v * v).sum::<f32>() / x.len().max(1) as f32).sqrt();
        assert!(x.len() > 16000 * 5, "{} samples of talker audio", x.len());
        let clipped = x.iter().filter(|v| v.abs() > 0.999).count();
        eprintln!("talker audio: {:.1} s, rms {rms:.4}, peak {:.3}, clipped {clipped}", x.len() as f32 / 16000.0, x.iter().fold(0f32, |m, v| m.max(v.abs())));
        assert!(rms > 0.005, "talker audio rms {rms}");
        assert!(clipped < x.len() / 1000, "{clipped} clipped samples");
    }

    #[test]
    fn listen_reports_the_audio_output() {
        let mut l = Live::new(args(&["--no-asr"]), None).unwrap();
        l.player_cmd = vec!["/no/such/player".into()];
        l.listen(Some(30.0));
        assert!(l.out.iter().any(|s| s.contains(r#""listen":"unavailable""#)), "{:?}", l.out);
        l.player_cmd = vec!["/bin/sh".into(), "-c".into(), "exec cat > /dev/null".into()];
        l.listen(Some(30.0));
        assert!(l.out.last().unwrap().contains(r#""listen":"ok""#), "{:?}", l.out);
        // a player that dies is reported (and dropped: the next choice starts a new one)
        l.listen(None);
        l.player_cmd = vec!["/bin/sh".into(), "-c".into(), "exit 0".into()];
        l.listen(Some(31.0));
        let t0 = std::time::Instant::now();
        while l.player.is_some() && t0.elapsed() < std::time::Duration::from_secs(2) {
            l.check_player();
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
        assert!(l.player.is_none());
        assert!(l.out.last().unwrap().contains(r#""listen":"unavailable""#), "{:?}", l.out);
    }

    #[test]
    fn live_matches_run_on_a_scene() {
        let Ok(root) = std::env::var("SCENE_ROOT") else {
            eprintln!("SCENE_ROOT unset: live_matches_run_on_a_scene skipped");
            return;
        };
        let dir = std::path::Path::new(&root).join("audio_wav_factory_4workers_9s5_b270_f150");
        let chans = crate::common::read_mics(&dir).unwrap();
        let len = chans[0].len();
        // file path, as `run --no-asr`
        let a = args(&["--no-asr"]);
        let active = crate::mics_active(&a.p).unwrap();
        let mut pipe = crate::build_pipeline(&a.p, &active, None).unwrap();
        let total = (len + crate::common::N).div_ceil(crate::common::H);
        for t in 0..total {
            let hops: Vec<Vec<f32>> = chans.iter().map(|c| (0..crate::common::H).map(|n| c.get(t * crate::common::H + n).copied().unwrap_or(0.0)).collect()).collect();
            let r: Vec<&[f32]> = hops.iter().map(|v| v.as_slice()).collect();
            pipe.process(&r);
        }
        let want: Vec<f32> = pipe.streams.iter().map(|s| s.az).collect();
        // the board path: file level x gain (16384/peak) x 256 in a ring file, back with scale 1/(256 gain)
        let peak = chans.iter().flat_map(|c| c.iter()).fold(0f32, |m, v| m.max(v.abs()));
        let gain = 16384.0 / peak;
        let path = std::env::temp_dir().join(format!("ap_live_{}", std::process::id()));
        let blocks = (total * crate::common::H / 64) as u64; // the same hops as the file run
        write_ring(&path, blocks, |w, fr, c| {
            let i = (w * 64) as usize + fr;
            let v = chans[c].get(i).copied().unwrap_or(0.0);
            // 24-bit, so the test checks the live plumbing, not the GUI's int16 re-quantisation
            (v * gain * 256.0).round() as i32
        });
        let mut l = Live::new(a, None).unwrap();
        let mut rd = crate::shm::ShmReader::open_at(&path, 0).unwrap();
        l.control(PassStart { pass: 1, scene: "s".into(), start: 5000, scale: 1.0 / (256.0 * gain) });
        loop {
            match rd.next().unwrap() {
                Read::Empty => break,
                r => l.on_read(r).unwrap(),
            }
        }
        let got: Vec<f32> = l.pipe.as_ref().unwrap().streams.iter().map(|s| s.az).collect();
        let _ = std::fs::remove_file(&path);
        assert_eq!(got.len(), want.len(), "live {got:?} vs run {want:?}");
        for (g, w) in got.iter().zip(&want) {
            assert!(crate::common::ang_diff(*g, *w).abs() < 0.5, "live {got:?} vs run {want:?}");
        }
    }

    /// A ring file holding `blocks` blocks (board index 5000 + 64 w), output sample from `f(w, frame, ch)`.
    fn write_ring(path: &std::path::Path, blocks: u64, f: impl Fn(u64, usize, usize) -> i32) {
        use std::os::unix::fs::FileExt;
        let file = std::fs::File::create(path).unwrap();
        let bb = crate::shm::BLOCK_BYTES;
        let mut h = [0u8; 64];
        h[0..8].copy_from_slice(b"PDMSHM01");
        h[8..12].copy_from_slice(&(blocks as u32 + 1).to_le_bytes()); // one free slot: a full ring reads as an overrun
        h[12..16].copy_from_slice(&(bb as u32).to_le_bytes());
        h[16..24].copy_from_slice(&blocks.to_le_bytes());
        let mut buf = h.to_vec();
        for w in 0..blocks {
            let mut b = vec![0u8; bb];
            b[0..8].copy_from_slice(&(5000 + 64 * w).to_le_bytes());
            for fr in 0..64 {
                for c in 0..8 {
                    let o = 8 + 64 * 8 * 2 + (fr * 8 + c) * 4;
                    b[o..o + 4].copy_from_slice(&f(w, fr, c).to_le_bytes());
                }
            }
            buf.extend_from_slice(&b);
        }
        buf.resize(buf.len() + bb, 0);
        file.write_all_at(&buf, 0).unwrap();
    }
}
