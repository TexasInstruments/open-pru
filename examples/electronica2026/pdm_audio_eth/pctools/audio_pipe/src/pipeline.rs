// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Frame-synchronous streaming pipeline (8 ms hop):
//! STFT -> WPE -> noise/SPP (ego-noise) -> DOA + tracking -> masks -> MVDR -> post-filter -> iSTFT -> endpointer -> ASR.
use crate::asr::{AsrWorker, Job};
use crate::common::*;
use crate::doa::{Doa, Geometry, Tracker};
use crate::noise::NoiseTracker;
use crate::sep::{SegEvent, SegParams, SepParams, Stream, posteriors, solve_all};
use crate::stft::Analyzer;
use crate::wpe::Wpe;
use std::collections::VecDeque;
use std::time::Instant;

/// Stage names of the per-frame profile (`Pipeline::prof`), in pipeline order.
pub const STAGES: [&str; 9] = [
    "stft",      // analysis STFT (+ reference-mic synthesis)
    "wpe",       // online WPE
    "noise_spp", // Φn tracking, SPP, Cholesky refresh, whitening
    "doa",       // whitened DOA voting, steering-grid re-whitening, peak picking
    "track",     // tracker, stream birth + replay, replay buffer upkeep
    "masks",     // spatial posteriors
    "cov",       // per-talker covariance update
    "mvdr",      // MVDR re-solve, steering update / re-whitening
    "post_out",  // beamform, Wiener + post-mask, iSTFT, endpointer
];
pub const NST: usize = STAGES.len();

#[inline]
fn lap(ts: &mut [f32; NST], k: usize, l: &mut Instant) {
    let n = Instant::now();
    ts[k] += (n - *l).as_secs_f32() * 1e6;
    *l = n;
}

pub struct PipeConfig {
    pub sep: SepParams,
    pub seg: SegParams,
    pub doa_every: usize,
    pub replay_frames: usize,
    pub white_load: f32,
    pub verbose: bool,
    /// context (frames) given to the neural second pass before an utterance
    pub ctx_frames: usize,
    /// first active mic (distortionless reference of the refined MVDR)
    pub ref_mic: usize,
    pub sep_mode: crate::ghdss::SepMode,
    pub ghdss: crate::ghdss::GhdssParams,
}

pub struct Pipeline {
    pub cfg: PipeConfig,
    geo: Geometry,
    ana: Analyzer,
    wpe: Wpe,
    pub noise: NoiseTracker,
    pub doa: Doa,
    pub tracker: Tracker,
    pub streams: Vec<Stream>,
    hist: VecDeque<(Frame, Frame, Vec<f32>)>, // (x̂, whitened x̂, p)
    pub t: usize,
    y: Frame,
    pub frame_us: Vec<f32>,
    /// per frame: µs spent in each stage (STAGES) and the number of live talker streams
    pub prof: Vec<([f32; NST], u8)>,
    pub asr: Option<AsrWorker>,
    pub jobs_sent: usize,
    pub doa_log: Vec<(usize, Vec<(f32, f32)>)>,
    pub act_log: Vec<Vec<f32>>, // per stream, per frame (aligned to global frames from birth replay)
    pub x_ref: Vec<f32>,        // WPE output, mic 1 (diagnostic)
    ref_synth: crate::stft::Synth,
    band: (usize, usize),
    pub refiner: Option<crate::refine::Refiner>,
    xring: VecDeque<Frame>, // x̂ history for the neural second pass
    xring_base: usize,      // frame index of xring[0]
    gh: crate::ghdss::GhdssRows,
    pub music: Option<crate::music::MusicRefiner>,
}

impl Pipeline {
    pub fn new(cfg: PipeConfig, geo: Geometry, wpe: Wpe, doa: Doa, tracker: Tracker, asr: Option<AsrWorker>) -> Self {
        let lo = (cfg.sep.act_lo * N as f32 / FS as f32).round() as usize;
        let hi = (cfg.sep.act_hi * N as f32 / FS as f32).round() as usize;
        let m_eff = cfg.sep.m_eff;
        let white_load = cfg.white_load;
        Self {
            cfg,
            geo,
            ana: Analyzer::new(),
            wpe,
            noise: NoiseTracker::new(m_eff, white_load),
            doa,
            tracker,
            streams: Vec::new(),
            hist: VecDeque::new(),
            t: 0,
            y: vec![ZV; F],
            frame_us: Vec::new(),
            prof: Vec::new(),
            asr,
            jobs_sent: 0,
            doa_log: Vec::new(),
            act_log: Vec::new(),
            x_ref: Vec::new(),
            ref_synth: crate::stft::Synth::new(),
            band: (lo, hi),
            refiner: None,
            xring: VecDeque::new(),
            xring_base: 0,
            gh: crate::ghdss::GhdssRows::new(),
            music: None,
        }
    }

    /// Samples of `src` (whose element 0 is input position `origin`) for input positions [p0, p1), zero outside.
    fn span(src: &[f32], origin: i64, p0: i64, p1: i64) -> Vec<f32> {
        (p0..p1)
            .map(|p| {
                let k = p - origin;
                if k >= 0 && (k as usize) < src.len() { src[k as usize] } else { 0.0 }
            })
            .collect()
    }

    fn activity(&self, p: &[f32], post: &[f32]) -> f32 {
        let (lo, hi) = self.band;
        (lo..=hi).map(|f| p[f] * post[f]).sum::<f32>() / (hi - lo + 1) as f32
    }

    /// Run one stream's per-frame output stage (apply, post-filter, synth, endpointer).
    fn stream_out(&mut self, si: usize, x: &Frame, p: &[f32], post: &[f32], t: usize) {
        let phin_out: Vec<f32> = (0..F).map(|f| self.streams[si].noise_out_power(&self.noise.phi_n(f), f, self.cfg.sep.m_eff)).collect();
        let act = self.activity(p, post);
        let sp = self.cfg.sep.clone();
        let st = &mut self.streams[si];
        st.apply(x, p, post, &phin_out, &sp);
        self.act_log[si].push(act);
        if let Some(ev) = st.seg.step(&self.cfg.seg, t, act) {
            self.dispatch(si, ev, t);
        }
    }

    fn dispatch(&mut self, si: usize, ev: SegEvent, now: usize) {
        let st = &self.streams[si];
        let (seg, start_frame, end_frame, partial) = match ev {
            SegEvent::Partial { seg, start_frame, now_frame } => (seg, start_frame, now_frame, true),
            SegEvent::Final { seg, start_frame, speech_end_frame } => (seg, start_frame, speech_end_frame, false),
        };
        // frame t's window covers input samples [(t+1)H - N, (t+1)H)
        let s0 = (start_frame as i64 + 1) * H as i64 - N as i64;
        let s1_want = (end_frame as i64 + 1) * H as i64 + 2 * H as i64; // a little tail after the last speech frame
        let avail = st.origin + st.pcm.len() as i64;
        let s1 = s1_want.min(avail);
        let a = (s0 - st.origin).max(0) as usize;
        let b = ((s1 - st.origin).max(0) as usize).min(st.pcm.len());
        if b <= a {
            return;
        }
        let pcm = st.pcm[a..b].to_vec();
        let t_start = s0.max(0) as f64 / FS as f64;
        let t_speech_end = ((end_frame + 1) * H) as f64 / FS as f64;
        if self.cfg.verbose {
            eprintln!(
                "[{:6.3}s] spk{} seg{} {} {:.2}-{:.2}s -> ASR",
                (now + 1) as f64 * H as f64 / FS as f64,
                st.id,
                seg,
                if partial { "partial" } else { "FINAL  " },
                t_start,
                t_speech_end
            );
        }
        let job = Job { speaker: st.id, seg, partial, pcm, t_start, t_speech_end, submitted: Instant::now(), refine_ms: 0.0 };
        if !partial && self.refiner.is_some() && now >= 3 {
            // neural second pass: utterance + context, x̂ frames t0..=t1
            let t0 = start_frame.saturating_sub(self.cfg.ctx_frames).max(self.xring_base);
            let t1 = (end_frame + 3).min(now - 3).min(self.xring_base + self.xring.len() - 1);
            if t1 > t0 {
                let p0 = (t0 as i64 + 1) * H as i64 - N as i64;
                let p1 = (t1 as i64 + 1) * H as i64;
                let frames: Vec<Frame> = (t0..=t1).map(|t| self.xring[t - self.xring_base].clone()).collect();
                let reference = Self::span(&self.x_ref, -((N - H) as i64), p0, p1);
                let bf = Self::span(&st.pcm, st.origin, p0, p1);
                let rj = crate::refine::RefineJob {
                    job,
                    frames,
                    reference,
                    bf,
                    steer: st.steer.clone(),
                    ref_mic: self.cfg.ref_mic,
                    seg_offset: (s0 - p0).max(0) as usize,
                    seg_len: (s1 - s0).max(0) as usize,
                };
                let _ = self.refiner.as_ref().unwrap().tx.send(rj);
                self.jobs_sent += 1;
                return;
            }
        }
        if let Some(asr) = &self.asr {
            let _ = asr.tx.send(job);
            self.jobs_sent += 1;
        }
    }

    fn spawn_stream(&mut self, track_id: usize, az_track: f32) {
        let t = self.t;
        let mut az = az_track;
        if let Some(mr) = &mut self.music {
            mr.refresh();
            if let Some(a) = mr.refine(&self.geo, az_track) {
                az = a;
            }
        }
        let replay: Vec<(Frame, Frame, Vec<f32>)> = self.hist.iter().cloned().collect();
        let first = t + 1 - replay.len() - 1; // frames in hist are t-len .. t-1; current frame t handled live after
        let id = self.streams.len();
        if self.cfg.verbose {
            eprintln!("[{:6.3}s] new speaker stream {id} at az {az:.0}° (replaying {} ms)", (t + 1) as f64 * H as f64 / FS as f64, replay.len() * H * 1000 / FS);
        }
        self.streams.push(Stream::new(id, track_id, az, &self.geo, first, t));
        self.streams[id].music_off = ang_diff(az, az_track);
        self.streams[id].refresh_white(&self.noise);
        self.act_log.push(Vec::new());
        if self.cfg.sep_mode == crate::ghdss::SepMode::Ghdss {
            if self.cfg.ghdss.init_mvdr {
                // same start as mask-MVDR: talker covariance from the replayed frames, one solve
                for (x, xw, p) in &replay {
                    let post = posteriors(xw, &self.streams, &self.cfg.sep);
                    let mask: Vec<f32> = (0..F).map(|f| p[f] * post[id][f]).collect();
                    self.streams[id].cov_update(x, &mask, self.cfg.sep.cov_gamma);
                }
                let phin: Vec<Mat8> = (0..F).map(|f| self.noise.phi_n(f)).collect();
                let rows: Vec<Vec<Vec8>> = self.streams.iter().map(|s| s.w.clone()).collect();
                solve_all(&mut self.streams, &phin, &self.cfg.sep);
                self.gh.add_stream_from_w(&self.streams[id]);
                for (k, r) in rows.into_iter().enumerate() {
                    if k != id {
                        self.streams[k].w = r; // keep the other talkers' GHDSS weights
                    }
                }
            } else {
                self.gh.add_stream(&self.streams[id]);
            }
            // adapt only the new talker's row over the replayed frames, then render them
            let upd: Vec<bool> = (0..self.streams.len()).map(|k| k == id).collect();
            for (k, (x, xw, p)) in replay.iter().enumerate() {
                self.gh.step(x, xw, &self.noise, &mut self.streams, &self.cfg.ghdss, Some(&upd));
                let post = posteriors(xw, &self.streams, &self.cfg.sep);
                self.stream_out(id, x, p, &post[id], first + k);
            }
            return;
        }
        // build this talker's covariance from the replayed frames, solve, then render them
        for (x, xw, p) in &replay {
            let post = posteriors(xw, &self.streams, &self.cfg.sep);
            let mask: Vec<f32> = (0..F).map(|f| p[f] * post[id][f]).collect();
            self.streams[id].cov_update(x, &mask, self.cfg.sep.cov_gamma);
        }
        let phin: Vec<Mat8> = (0..F).map(|f| self.noise.phi_n(f)).collect();
        solve_all(&mut self.streams, &phin, &self.cfg.sep);
        for (k, (x, xw, p)) in replay.iter().enumerate() {
            let post = posteriors(xw, &self.streams, &self.cfg.sep);
            self.stream_out(id, x, p, &post[id], first + k);
        }
    }

    /// Process one hop of 8-channel input.
    pub fn process(&mut self, hop: &[&[f32]]) {
        let t0 = Instant::now();
        let mut ts = [0f32; NST];
        let mut l = t0;
        let t = self.t;
        let mut y = std::mem::take(&mut self.y);
        self.ana.push(hop, &mut y);
        lap(&mut ts, 0, &mut l);
        self.wpe.process(&mut y);
        lap(&mut ts, 1, &mut l);
        let x = y; // dereverberated
        {
            let r: Vec<C> = x.iter().map(|v| v[1]).collect(); // mic 2 (mic 1 may be switched off)
            self.ref_synth.push(&r, &mut self.x_ref);
        }
        lap(&mut ts, 0, &mut l);
        self.noise.update(&x);
        lap(&mut ts, 2, &mut l);
        if let Some(mr) = &mut self.music {
            mr.update(&x);
            if t % mr.every == 0 && t > 32 && !self.streams.is_empty() {
                mr.refresh();
                for (si, st) in self.streams.iter_mut().enumerate() {
                    // only while this talker is speaking: otherwise the window holds other sources only
                    let recent = &self.act_log[si][self.act_log[si].len().saturating_sub(mr.every)..];
                    let act = recent.iter().sum::<f32>() / recent.len().max(1) as f32;
                    if act < 2.0 * self.cfg.seg.on {
                        continue;
                    }
                    if let Some(tr) = self.tracker.tracks.iter().find(|tr| tr.id == st.track_id) {
                        if let Some(a) = mr.refine_gated(&self.geo, wrap_deg(tr.az + st.music_off), 1.15) {
                            st.music_off += mr.alpha * ang_diff(a, wrap_deg(tr.az + st.music_off));
                            st.music_off = st.music_off.clamp(-mr.span, mr.span);
                        }
                    }
                }
            }
            lap(&mut ts, 3, &mut l);
        }
        if t % self.cfg.doa_every == 0 && t > 16 {
            self.noise.refresh();
            lap(&mut ts, 2, &mut l);
            self.doa.refresh_white(&self.noise);
            lap(&mut ts, 3, &mut l);
            for st in self.streams.iter_mut() {
                st.refresh_white(&self.noise);
            }
            lap(&mut ts, 7, &mut l);
        }
        let p = self.noise.p.clone();
        let xw: Frame = (0..F).map(|f| self.noise.whiten(f, &x[f])).collect();
        lap(&mut ts, 2, &mut l);
        if matches!(self.noise.ego_mode, crate::noise::EgoMode::Doa | crate::noise::EgoMode::AddDoa) && self.noise.ego.is_some() {
            let xd: Frame = (0..F).map(|f| self.noise.whiten_doa(f, &x[f])).collect();
            self.doa.accumulate(&xd, &p);
        } else {
            self.doa.accumulate(&xw, &p);
        }
        lap(&mut ts, 3, &mut l);
        if t % self.cfg.doa_every == 0 {
            let peaks = self.doa.peaks();
            if self.cfg.verbose && !peaks.is_empty() {
                self.doa_log.push((t, peaks.clone()));
            }
            lap(&mut ts, 3, &mut l);
            let newly = self.tracker.update(t, &peaks);
            for tid in newly {
                let az = self.tracker.tracks.iter().find(|tr| tr.id == tid).unwrap().az;
                self.spawn_stream(tid, az);
            }
            lap(&mut ts, 4, &mut l);
            for si in 0..self.streams.len() {
                let tid = self.streams[si].track_id;
                if let Some(tr) = self.tracker.tracks.iter().find(|tr| tr.id == tid) {
                    let az = wrap_deg(tr.az + self.streams[si].music_off);
                    if self.streams[si].set_az(az, &self.geo) {
                        self.streams[si].refresh_white(&self.noise);
                    }
                }
            }
            lap(&mut ts, 7, &mut l);
        }
        if !self.streams.is_empty() {
            let post = posteriors(&xw, &self.streams, &self.cfg.sep);
            lap(&mut ts, 5, &mut l);
            let ghdss = self.cfg.sep_mode == crate::ghdss::SepMode::Ghdss;
            if ghdss {
                self.gh.step(&x, &xw, &self.noise, &mut self.streams, &self.cfg.ghdss, None);
                lap(&mut ts, 7, &mut l);
            }
            for si in 0..self.streams.len() {
                if ghdss && !self.cfg.ghdss.init_mvdr {
                    break;
                }
                let mask: Vec<f32> = (0..F).map(|f| p[f] * post[si][f]).collect();
                self.streams[si].cov_update(&x, &mask, self.cfg.sep.cov_gamma);
            }
            lap(&mut ts, 6, &mut l);
            if !ghdss && t % self.cfg.sep.solve_every == 0 {
                let phin: Vec<Mat8> = (0..F).map(|f| self.noise.phi_n(f)).collect();
                solve_all(&mut self.streams, &phin, &self.cfg.sep);
            }
            lap(&mut ts, 7, &mut l);
            for si in 0..self.streams.len() {
                self.stream_out(si, &x, &p, &post[si], t);
            }
            lap(&mut ts, 8, &mut l);
        }
        self.hist.push_back((x.clone(), xw, p));
        if self.refiner.is_some() {
            self.xring.push_back(x.clone());
            let cap = self.cfg.seg.max_frames + self.cfg.ctx_frames + 16;
            while self.xring.len() > cap {
                self.xring.pop_front();
                self.xring_base += 1;
            }
        }
        if self.hist.len() > self.cfg.replay_frames {
            self.hist.pop_front();
        }
        lap(&mut ts, 4, &mut l);
        self.y = x;
        self.t += 1;
        self.frame_us.push(t0.elapsed().as_secs_f32() * 1e6);
        self.prof.push((ts, self.streams.len() as u8));
    }

    /// State bytes per stage (STAGES order), for the current number of streams. Excludes the growing output pcm.
    pub fn mem_bytes(&self) -> [usize; NST] {
        let mut b = [0usize; NST];
        b[0] = self.ana.mem_bytes() + self.ref_synth.mem_bytes();
        b[1] = self.wpe.mem_bytes();
        b[2] = self.noise.mem_bytes();
        b[3] = self.doa.mem_bytes() + self.music.as_ref().map_or(0, |m| m.mem_bytes());
        // replay buffer: replay_frames × (x̂, whitened x̂, SPP), at capacity
        let per = F * (2 * std::mem::size_of::<Vec8>() + 4);
        b[4] = self.cfg.replay_frames * per + self.tracker.tracks.len() * 64 + self.xring.len() * F * std::mem::size_of::<Vec8>();
        b[7] += self.gh.rows.iter().map(|r| vbytes(r)).sum::<usize>();
        for st in &self.streams {
            let m = st.mem_bytes();
            b[5] += m[0];
            b[6] += m[1];
            b[7] += m[2];
            b[8] += m[3];
        }
        b
    }

    pub fn flush(&mut self) {
        for si in 0..self.streams.len() {
            if let Some(ev) = self.streams[si].seg.flush(&self.cfg.seg) {
                let t = self.t;
                self.dispatch(si, ev, t);
            }
        }
    }
}
