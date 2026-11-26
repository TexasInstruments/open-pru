// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Second pass on each finished utterance, with a pretrained neural separator (SpeechBrain SepFormer, WHAMR! 16 kHz)
//! running in a Python sidecar on the GPU (`py/mask_server.py`).
//!
//! The streaming path keeps its low latency (partials, endpointing). When an utterance ends, the Refiner thread
//! 1. sends the WPE'd reference mic + this talker's streaming beamformer output to the sidecar,
//! 2. gets back the T-F mask of the separated source that matches this talker (permutation solved by spectrogram
//!    correlation with the stream) plus the separated waveform,
//! 3. `--neural mvdr`: re-estimates the talker / interference covariances from that mask over the whole utterance
//!    (batch, non-causal), takes the talker's RTF from the GEV principal eigenvector, runs MVDR on the 8-ch x̂, applies
//!    a mild neural post-mask, iSTFT.  `--neural sep`: uses the separated waveform directly.
//! 4. hands the audio to the ASR worker.
use crate::asr::Job;
use crate::common::*;
use crate::stft::Synth;
use anyhow::{Context, Result, anyhow, bail};
use crossbeam_channel::{Receiver, Sender, unbounded};
use std::io::{BufRead, BufReader, Read, Write};
use std::path::Path;
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use std::thread::JoinHandle;
use std::time::Instant;

#[derive(Clone, Copy, PartialEq, Debug, clap::ValueEnum)]
pub enum NeuralMode {
    /// no neural second pass
    Off,
    /// neural-mask MVDR on the 8-ch dereverberated STFT (the note's sec. 8 with the sec. 7 mask)
    Mvdr,
    /// ASR directly on the separated single-channel waveform
    Sep,
}

pub struct NeuralClient {
    child: Child,
    stdin: ChildStdin,
    stdout: BufReader<ChildStdout>,
}

pub struct NeuralOut {
    pub mask: Vec<f32>, // frames x F
    pub frames: usize,
    pub score: f32,
    pub sep: Vec<f32>,
}

impl NeuralClient {
    pub fn spawn(python: &Path, script: &Path) -> Result<Self> {
        let mut child = Command::new(python)
            .arg(script)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()
            .with_context(|| format!("spawn {} {}", python.display(), script.display()))?;
        let stdin = child.stdin.take().unwrap();
        let mut stdout = BufReader::new(child.stdout.take().unwrap());
        let mut line = String::new();
        stdout.read_line(&mut line)?;
        if line.trim() != "READY" {
            bail!("mask server did not start (got {line:?})");
        }
        Ok(Self { child, stdin, stdout })
    }

    pub fn request(&mut self, reference: &[f32], bf: &[f32]) -> Result<NeuralOut> {
        let mut msg = Vec::with_capacity(12 + 4 * (reference.len() + bf.len()));
        msg.extend_from_slice(b"MASK");
        msg.extend_from_slice(&(reference.len() as u32).to_le_bytes());
        msg.extend_from_slice(&(bf.len() as u32).to_le_bytes());
        for v in reference.iter().chain(bf.iter()) {
            msg.extend_from_slice(&v.to_le_bytes());
        }
        self.stdin.write_all(&msg)?;
        self.stdin.flush()?;
        let mut hdr = [0u8; 16];
        self.stdout.read_exact(&mut hdr)?;
        if &hdr[..4] != b"MASK" {
            return Err(anyhow!("mask server protocol error"));
        }
        let frames = u32::from_le_bytes(hdr[4..8].try_into().unwrap()) as usize;
        let nf = u32::from_le_bytes(hdr[8..12].try_into().unwrap()) as usize;
        let score = f32::from_le_bytes(hdr[12..16].try_into().unwrap());
        anyhow::ensure!(nf == F, "mask server F={nf}, expected {F}");
        let mask = read_f32s(&mut self.stdout, frames * F)?;
        let mut n = [0u8; 4];
        self.stdout.read_exact(&mut n)?;
        let sep = read_f32s(&mut self.stdout, u32::from_le_bytes(n) as usize)?;
        Ok(NeuralOut { mask, frames, score, sep })
    }
}

impl Drop for NeuralClient {
    fn drop(&mut self) {
        let _ = self.stdin.write_all(b"QUIT");
        let _ = self.stdin.flush();
        let _ = self.child.wait();
    }
}

fn read_f32s(r: &mut impl Read, n: usize) -> Result<Vec<f32>> {
    let mut b = vec![0u8; 4 * n];
    r.read_exact(&mut b)?;
    Ok(b.chunks_exact(4).map(|c| f32::from_le_bytes(c.try_into().unwrap())).collect())
}

pub struct RefineJob {
    pub job: Job,             // the ASR job as the streaming path built it (pcm = streaming output)
    pub frames: Vec<Frame>,   // x̂ for Rust frames t0 ..= t1
    pub reference: Vec<f32>,  // WPE'd mic, input positions [(t0+1)H - N, (t1+1)H)
    pub bf: Vec<f32>,         // streaming beamformer output, same span
    pub steer: Vec<Vec8>,     // geometric steering of this talker (fallback / eigen start)
    pub ref_mic: usize,       // first active mic: MVDR distortionless reference
    pub seg_offset: usize,    // samples from span start to the ASR segment start
    pub seg_len: usize,
}

pub struct Refiner {
    pub tx: Sender<RefineJob>,
    handle: Option<JoinHandle<()>>,
}

pub struct RefineParams {
    pub mode: NeuralMode,
    pub post_floor: f32,
    pub loading: f32,
}

impl Refiner {
    pub fn spawn(client: NeuralClient, asr_tx: Sender<Job>, prm: RefineParams, verbose: bool) -> Self {
        let (tx, rx): (Sender<RefineJob>, Receiver<RefineJob>) = unbounded();
        let handle = std::thread::spawn(move || {
            let mut client = client;
            for rj in rx.iter() {
                let t0 = Instant::now();
                let mut job = rj.job;
                match client.request(&rj.reference, &rj.bf) {
                    Ok(out) => {
                        let t_nn = t0.elapsed().as_secs_f64() * 1e3;
                        let pcm = match prm.mode {
                            NeuralMode::Sep => out.sep.clone(),
                            _ => mask_mvdr(&rj.frames, &out, &rj.steer, rj.ref_mic, &prm),
                        };
                        let a = rj.seg_offset.min(pcm.len());
                        let b = (rj.seg_offset + rj.seg_len).min(pcm.len());
                        if b > a {
                            job.pcm = pcm[a..b].to_vec();
                        }
                        job.refine_ms = t0.elapsed().as_secs_f64() * 1e3;
                        if verbose {
                            eprintln!(
                                "  refine spk{} seg{}: neural {:.0} ms (perm. score {:.2}), total {:.0} ms",
                                job.speaker, job.seg, t_nn, out.score, job.refine_ms
                            );
                        }
                    }
                    Err(e) => eprintln!("  refine spk{} seg{} failed ({e}); using streaming audio", job.speaker, job.seg),
                }
                if asr_tx.send(job).is_err() {
                    return;
                }
            }
        });
        Self { tx, handle: Some(handle) }
    }

    pub fn finish(mut self) {
        drop(self.tx);
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
    }
}

/// Batch mask-based MVDR over one utterance. Φc = Σ M x xᴴ / Σ M, Φi = Σ (1-M) x xᴴ / Σ (1-M);
/// RTF d = Φi v, v = principal eigenvector of Φi⁻¹Φc (GEV), normalised to d[ref] = 1; w = Φi⁻¹d / dᴴΦi⁻¹d.
fn mask_mvdr(frames: &[Frame], out: &NeuralOut, steer: &[Vec8], ref_mic: usize, prm: &RefineParams) -> Vec<f32> {
    let nt = frames.len().min(out.frames);
    let mut w = vec![ZV; F];
    for f in 1..F {
        let (mut pc, mut pi) = (ZM, ZM);
        let (mut sc, mut si) = (0f32, 0f32);
        for t in 0..nt {
            let m = out.mask[t * F + f];
            rank1_acc(&mut pc, &frames[t][f], m);
            rank1_acc(&mut pi, &frames[t][f], 1.0 - m);
            sc += m;
            si += 1.0 - m;
        }
        scale_mat(&mut pc, 1.0 / sc.max(1e-6));
        scale_mat(&mut pi, 1.0 / si.max(1e-6));
        let load = prm.loading * trace_re(&pi) / M as f32 + 1e-12;
        // power iteration on Φi⁻¹Φc, started from the geometric steering vector
        let mut v = steer[f];
        let mut ok = true;
        for _ in 0..8 {
            let mut u = ZV;
            for i in 0..M {
                for j in 0..M {
                    u[i] += pc[i][j] * v[j];
                }
            }
            match herm_solve(&pi, load, &u) {
                Some(s) => {
                    let n = s.iter().map(|c| c.norm_sqr()).sum::<f32>().sqrt();
                    if !(n > 0.0 && n.is_finite()) {
                        ok = false;
                        break;
                    }
                    v = core::array::from_fn(|m| s[m] / n);
                }
                None => {
                    ok = false;
                    break;
                }
            }
        }
        let mut d = steer[f];
        if ok {
            let mut h = ZV;
            for i in 0..M {
                for j in 0..M {
                    h[i] += pi[i][j] * v[j];
                }
            }
            if h[ref_mic].norm() > 1e-12 {
                let r = h[ref_mic];
                d = core::array::from_fn(|m| h[m] / r);
            }
        }
        w[f] = match herm_solve(&pi, load, &d) {
            Some(q) => {
                let den = dot_h(&d, &q).re;
                if den > 0.0 && den.is_finite() { core::array::from_fn(|m| q[m] / den) } else { ZV }
            }
            None => ZV,
        };
    }
    let mut syn = Synth::new();
    let mut pcm = Vec::with_capacity(nt * H + N);
    let mut spec = vec![ZERO; F];
    for t in 0..nt {
        for f in 0..F {
            let g = out.mask[t * F + f].max(prm.post_floor).min(1.0);
            spec[f] = dot_h(&w[f], &frames[t][f]) * g;
        }
        syn.push(&spec, &mut pcm);
    }
    // frame t emits the H samples starting at its window start, so pcm[0] is already the span start;
    // flush the last (N - H) samples of the tail with empty frames
    for _ in 0..(N - H) / H {
        syn.push(&vec![ZERO; F], &mut pcm);
    }
    pcm
}
