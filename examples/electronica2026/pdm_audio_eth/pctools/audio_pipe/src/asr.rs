// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Whisper (whisper.cpp, CUDA) ASR worker. Runs on its own thread; jobs in, results out.
//! Partial jobs are best-effort (stale partials are skipped); final jobs are always decoded.
use anyhow::{Result, anyhow};
use crossbeam_channel::{Receiver, Sender, unbounded};
use std::path::{Path, PathBuf};
use std::thread::JoinHandle;
use std::time::Instant;
use whisper_rs::{FullParams, SamplingStrategy, WhisperContext, WhisperContextParameters, WhisperState};

use crate::common::FS;

pub struct Job {
    pub speaker: usize,
    pub seg: usize,
    pub partial: bool,
    pub pcm: Vec<f32>,
    /// audio time (s) of the segment start / of the last speech frame
    pub t_start: f64,
    pub t_speech_end: f64,
    pub submitted: Instant,
    /// time spent in the neural second pass before ASR (0 if none)
    pub refine_ms: f64,
}

/// One decoded hypothesis with whisper's confidence measures.
#[derive(Clone, Debug, Default)]
pub struct Hyp {
    pub text: String,
    pub no_speech: f32,
    pub avg_p: f32,
    pub accepted: bool,
}

/// Strip non-speech annotations whisper emits for noise ("*sad music*", "[Music]", "(laughs)") and
/// decide whether the hypothesis is speech.
pub fn accept(mut h: Hyp, max_no_speech: f32, min_avg_p: f32) -> Hyp {
    let mut out = String::new();
    let mut depth = 0i32;
    let mut star = false;
    for ch in h.text.chars() {
        match ch {
            '[' | '(' => depth += 1,
            ']' | ')' => depth = (depth - 1).max(0),
            '*' => star = !star,
            _ if depth == 0 && !star => out.push(ch),
            _ => {}
        }
    }
    let cleaned = out.split_whitespace().collect::<Vec<_>>().join(" ");
    let has_words = cleaned.chars().filter(|c| c.is_alphabetic()).count() >= 2;
    h.accepted = has_words && h.no_speech <= max_no_speech && h.avg_p >= min_avg_p;
    h.text = cleaned.trim_matches(|c: char| c == '-' || c.is_whitespace()).to_string();
    h
}

pub struct AsrResult {
    pub speaker: usize,
    pub seg: usize,
    pub partial: bool,
    pub text: String,
    pub hyp: Hyp,
    pub t_start: f64,
    pub t_speech_end: f64,
    pub submitted: Instant,
    pub done: Instant,
    pub decode_ms: f64,
    pub refine_ms: f64,
}

pub struct Transcriber {
    _ctx: WhisperContext,
    state: WhisperState,
    pub language: String,
    pub threads: i32,
}

impl Transcriber {
    pub fn new(model: &Path, gpu: bool, language: &str) -> Result<Self> {
        whisper_rs::install_logging_hooks();
        let mut cp = WhisperContextParameters::default();
        cp.use_gpu(gpu);
        let ctx = WhisperContext::new_with_params(model, cp).map_err(|e| anyhow!("load {}: {e:?}", model.display()))?;
        let state = ctx.create_state().map_err(|e| anyhow!("whisper state: {e:?}"))?;
        Ok(Self { _ctx: ctx, state, language: language.to_string(), threads: 4 })
    }

    /// Transcribe 16 kHz mono PCM. Short clips are zero-padded to 1.1 s (whisper.cpp rejects < 1 s).
    pub fn transcribe(&mut self, pcm: &[f32]) -> Result<String> {
        Ok(self.decode(pcm)?.text)
    }

    pub fn decode(&mut self, pcm: &[f32]) -> Result<Hyp> {
        let mut audio = Vec::with_capacity(pcm.len() + FS);
        let pad = FS / 10; // 100 ms of leading/trailing silence
        audio.resize(pad, 0.0);
        audio.extend_from_slice(pcm);
        let min_len = FS * 11 / 10;
        audio.resize((audio.len() + pad).max(min_len), 0.0);
        // level-normalise to -20 dBFS RMS (beamformer outputs are often quiet), clip-safe
        let rms = (audio.iter().map(|v| v * v).sum::<f32>() / audio.len() as f32).sqrt();
        let pk = audio.iter().fold(0f32, |a, &v| a.max(v.abs()));
        if rms > 1e-7 {
            let g = (0.1 / rms).min(0.95 / pk.max(1e-9));
            audio.iter_mut().for_each(|v| *v *= g);
        }
        let mut p = FullParams::new(SamplingStrategy::Greedy { best_of: 1 });
        p.set_language(Some(&self.language));
        p.set_n_threads(self.threads);
        p.set_print_progress(false);
        p.set_print_realtime(false);
        p.set_print_special(false);
        p.set_print_timestamps(false);
        p.set_no_context(true);
        p.set_single_segment(true);
        p.set_suppress_blank(true);
        p.set_no_timestamps(true);
        p.set_temperature_inc(0.0); // no fallback re-decodes: bounded latency
        self.state.full(p, &audio).map_err(|e| anyhow!("whisper full: {e:?}"))?;
        let mut text = String::new();
        let (mut no_speech, mut psum, mut np) = (0f32, 0f32, 0usize);
        for i in 0..self.state.full_n_segments() {
            if let Some(s) = self.state.get_segment(i) {
                text.push_str(&s.to_str_lossy().map_err(|e| anyhow!("{e:?}"))?);
                no_speech = no_speech.max(s.no_speech_probability());
                for k in 0..s.n_tokens() {
                    if let Some(tok) = s.get_token(k) {
                        let special = tok.to_str_lossy().map(|t| t.starts_with("[_") || t.starts_with("<|")).unwrap_or(true);
                        if !special {
                            psum += tok.token_probability();
                            np += 1;
                        }
                    }
                }
            }
        }
        Ok(Hyp { text: text.trim().to_string(), no_speech, avg_p: if np > 0 { psum / np as f32 } else { 0.0 }, accepted: true })
    }
}

pub struct AsrWorker {
    pub tx: Sender<Job>,
    pub rx: Receiver<AsrResult>,
    handle: Option<JoinHandle<()>>,
}

impl AsrWorker {
    pub fn spawn(model: PathBuf, gpu: bool, language: String) -> Result<Self> {
        let (tx, jrx) = unbounded::<Job>();
        let (rtx, rx) = unbounded::<AsrResult>();
        let (ready_tx, ready_rx) = crossbeam_channel::bounded::<Result<()>>(1);
        let handle = std::thread::spawn(move || {
            let mut tr = match Transcriber::new(&model, gpu, &language) {
                Ok(t) => {
                    // warm-up: first CUDA call allocates buffers / compiles graphs
                    let _ = t;
                    let mut t = t;
                    let _ = t.transcribe(&vec![0.0; FS]);
                    let _ = ready_tx.send(Ok(()));
                    t
                }
                Err(e) => {
                    let _ = ready_tx.send(Err(e));
                    return;
                }
            };
            while let Ok(first) = jrx.recv() {
                // drain: keep all finals, keep only the newest partial per speaker if no final for it is queued
                let mut batch = vec![first];
                while let Ok(j) = jrx.try_recv() {
                    batch.push(j);
                }
                let mut keep: Vec<Job> = Vec::new();
                for j in batch.into_iter().rev() {
                    if j.partial && keep.iter().any(|k| k.speaker == j.speaker && k.seg == j.seg) {
                        continue; // a newer partial or the final of this segment is already queued
                    }
                    keep.push(j);
                }
                keep.reverse();
                for j in keep {
                    let t0 = Instant::now();
                    let hyp = tr.decode(&j.pcm).unwrap_or_else(|e| Hyp { text: format!("<asr error: {e}>"), ..Default::default() });
                    let hyp = accept(hyp, 0.6, 0.35);
                    let text = if hyp.accepted { hyp.text.clone() } else { String::new() };
                    let done = Instant::now();
                    let r = AsrResult {
                        speaker: j.speaker,
                        seg: j.seg,
                        partial: j.partial,
                        text,
                        hyp,
                        t_start: j.t_start,
                        t_speech_end: j.t_speech_end,
                        submitted: j.submitted,
                        done,
                        decode_ms: (done - t0).as_secs_f64() * 1e3,
                        refine_ms: j.refine_ms,
                    };
                    if rtx.send(r).is_err() {
                        return;
                    }
                }
            }
        });
        ready_rx.recv().map_err(|_| anyhow!("ASR thread died during start-up"))??;
        Ok(Self { tx, rx, handle: Some(handle) })
    }

    /// Close the job queue and wait for the worker to finish everything queued.
    pub fn finish(mut self) -> Vec<AsrResult> {
        drop(self.tx);
        let mut out: Vec<AsrResult> = Vec::new();
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
        while let Ok(r) = self.rx.try_recv() {
            out.push(r);
        }
        out
    }
}

#[cfg(test)]
impl Job {
    pub fn test_final(speaker: usize, t_start: f64, t_speech_end: f64) -> Self {
        Job { speaker, seg: 0, partial: false, pcm: vec![], t_start, t_speech_end, submitted: Instant::now(), refine_ms: 0.0 }
    }
}

#[cfg(test)]
impl AsrResult {
    pub fn test_final(speaker: usize, t_start: f64, t_speech_end: f64, text: &str) -> Self {
        let now = Instant::now();
        let hyp = Hyp { text: text.into(), accepted: true, ..Default::default() };
        AsrResult { speaker, seg: 0, partial: false, text: text.into(), hyp, t_start, t_speech_end, submitted: now, done: now, decode_ms: 0.0, refine_ms: 0.0 }
    }
}

#[cfg(test)]
impl AsrWorker {
    /// A worker without a model: every job comes back accepted as "speaker <n>".
    pub fn echo() -> Self {
        let (tx, jrx) = unbounded::<Job>();
        let (rtx, rx) = unbounded::<AsrResult>();
        let handle = std::thread::spawn(move || {
            while let Ok(j) = jrx.recv() {
                let mut r = AsrResult::test_final(j.speaker, j.t_start, j.t_speech_end, &format!("speaker {}", j.speaker));
                r.seg = j.seg;
                if rtx.send(r).is_err() {
                    return;
                }
            }
        });
        Self { tx, rx, handle: Some(handle) }
    }
}
