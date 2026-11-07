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

    /// A pass start from the GUI. One the reader has already passed is skipped.
    pub fn control(&mut self, ps: PassStart) {
        if let Some(n) = self.next {
            if ps.start < n {
                eprintln!("live: pass {} start {} is behind the reader ({n}), skipped", ps.pass, ps.start);
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
                            self.pending.retain(|q| q.start >= b.index);
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
            }
        }
        Ok(())
    }

    fn begin_pass(&mut self, q: PassStart) -> Result<()> {
        self.end_pass(true);
        let pipe = crate::build_pipeline(&self.p, &self.active, self.asr.take())?;
        eprintln!("live: pass {} ({}) from board index {}", q.pass, q.scene, q.start);
        self.pipe = Some(pipe);
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
        while let Ok(r) = rx.try_recv() {
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
                Ok(l) => match handle_control(&l) {
                    Some(ps) => live.control(ps),
                    None => eprintln!("live: control line ignored: {}", l.trim()),
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
