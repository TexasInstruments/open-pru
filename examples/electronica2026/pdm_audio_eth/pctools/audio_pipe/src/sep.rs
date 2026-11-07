// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Per-speaker separation: spatial masks -> mask-based MVDR -> Wiener post-filter -> iSTFT, plus the endpointer
//! that cuts each speaker stream into utterances for ASR (impl. note sec. 7-9).
//!
//! Masks: the note puts a trained causal GRU/TCN on the NPU here. No trained network ships with this repo, so masks
//! come from causal spatial clustering instead: per bin, a Watson-like posterior over the tracked talkers
//! (plus a background class), times the speech-presence probability. Same interface (M_c(t,f) ∈ [0,1]).
use crate::common::*;
use crate::doa::Geometry;
use crate::stft::Synth;

#[derive(Clone)]
pub struct SepParams {
    pub kappa: f32,        // posterior sharpness
    pub s_bg: f32,         // coherence of the background class
    pub cov_gamma: f32,    // talker covariance forgetting
    pub loading: f32,      // diagonal loading (fraction of tr/M)
    pub solve_every: usize,
    pub pf_gmin: f32,      // Wiener floor
    pub sp_floor: f32,     // spatial post-mask floor (1 = off)
    pub act_lo: f32,
    pub act_hi: f32,
    pub m_eff: f32,
}

pub struct SegParams {
    pub on: f32,
    pub off: f32,
    pub hang_frames: usize,
    pub pre_frames: usize,
    pub min_frames: usize,
    pub max_frames: usize,
    pub partial_frames: usize, // 0 = no partial hypotheses
}

pub enum SegEvent {
    Partial { seg: usize, start_frame: usize, now_frame: usize },
    Final { seg: usize, start_frame: usize, speech_end_frame: usize },
}

pub struct Segmenter {
    active: bool,
    start: usize,
    last_speech: usize,
    last_partial: usize,
    speech_frames: usize,
    pub n_seg: usize,
    sm: f32,
}

impl Segmenter {
    pub fn new() -> Self {
        Self { active: false, start: 0, last_speech: 0, last_partial: 0, speech_frames: 0, n_seg: 0, sm: 0.0 }
    }
    pub fn step(&mut self, p: &SegParams, t: usize, activity: f32) -> Option<SegEvent> {
        let activity = if activity.is_finite() { activity } else { 0.0 };
        self.sm = 0.5 * self.sm + 0.5 * activity;
        let speech = if self.active { self.sm > p.off } else { self.sm > p.on };
        if !self.active {
            if speech {
                self.active = true;
                self.start = t.saturating_sub(p.pre_frames);
                self.last_speech = t;
                self.last_partial = t;
                self.speech_frames = 1;
            }
            return None;
        }
        if speech {
            self.last_speech = t;
            self.speech_frames += 1;
        }
        let ended = t - self.last_speech >= p.hang_frames || t - self.start >= p.max_frames;
        if ended {
            self.active = false;
            if self.speech_frames >= p.min_frames {
                let seg = self.n_seg;
                self.n_seg += 1;
                return Some(SegEvent::Final { seg, start_frame: self.start, speech_end_frame: self.last_speech });
            }
            return None;
        }
        if p.partial_frames > 0 && t - self.last_partial >= p.partial_frames && self.speech_frames >= p.min_frames {
            self.last_partial = t;
            return Some(SegEvent::Partial { seg: self.n_seg, start_frame: self.start, now_frame: t });
        }
        None
    }
    /// Force-close at end of input.
    pub fn flush(&mut self, p: &SegParams) -> Option<SegEvent> {
        if self.active && self.speech_frames >= p.min_frames {
            self.active = false;
            let seg = self.n_seg;
            self.n_seg += 1;
            return Some(SegEvent::Final { seg, start_frame: self.start, speech_end_frame: self.last_speech });
        }
        None
    }
}

pub struct Stream {
    pub id: usize,
    pub track_id: usize,
    pub az: f32,
    steer_az: f32,
    pub steer: Vec<Vec8>, // per bin
    pub steer_w: Vec<Vec8>, // whitened L⁻¹d per bin
    steer_wn: Vec<f32>,
    acc: Vec<Mat8>,
    wsum: Vec<f32>,
    pub w: Vec<Vec8>,
    pub have_w: bool,
    pf_prev: Vec<f32>,
    synth: Synth,
    pub pcm: Vec<f32>,
    /// input-sample position of pcm[0] (may be negative for the first frames)
    pub origin: i64,
    pub seg: Segmenter,
    pub born_frame: usize,
    spec: Vec<C>,
    /// MUSIC azimuth correction relative to the tracker (deg, smoothed)
    pub music_off: f32,
}

impl Stream {
    pub fn new(id: usize, track_id: usize, az: f32, geo: &Geometry, first_frame: usize, born_frame: usize) -> Self {
        let steer: Vec<Vec8> = (0..F).map(|f| geo.steer(az, f)).collect();
        Self {
            steer_w: steer.clone(),
            steer_wn: vec![1.0; F],
            id,
            track_id,
            az,
            steer_az: az,
            steer,
            acc: vec![ZM; F],
            wsum: vec![0.0; F],
            w: vec![ZV; F],
            have_w: false,
            pf_prev: vec![1.0; F],
            synth: Synth::new(),
            pcm: Vec::new(),
            origin: (first_frame as i64 + 1) * H as i64 - N as i64,
            seg: Segmenter::new(),
            born_frame,
            spec: vec![ZERO; F],
            music_off: 0.0,
        }
    }

    /// State bytes split as (masks: whitened steering, covariance: Φc accumulator, MVDR: steering + weights,
    /// output: post-filter state + synthesis). The pcm output buffer is reported separately.
    pub fn mem_bytes(&self) -> [usize; 4] {
        [
            vbytes(&self.steer_w) + vbytes(&self.steer_wn),
            vbytes(&self.acc) + vbytes(&self.wsum),
            vbytes(&self.steer) + vbytes(&self.w),
            vbytes(&self.pf_prev) + vbytes(&self.spec) + self.synth.mem_bytes() + std::mem::size_of::<Segmenter>(),
        ]
    }

    /// Returns true if the steering vectors changed (caller re-whitens).
    pub fn set_az(&mut self, az: f32, geo: &Geometry) -> bool {
        self.az = az;
        if ang_diff(az, self.steer_az).abs() > 1.0 {
            self.steer_az = az;
            for f in 0..F {
                self.steer[f] = geo.steer(az, f);
            }
            return true;
        }
        false
    }

    pub fn refresh_white(&mut self, noise: &crate::noise::NoiseTracker) {
        for f in 0..F {
            let aw = fwd_sub(&noise.chol[f], &self.steer[f]);
            self.steer_wn[f] = aw.iter().map(|c| c.norm_sqr()).sum::<f32>().max(1e-30);
            self.steer_w[f] = aw;
        }
    }

    pub fn cov_update(&mut self, x: &Frame, mask: &[f32], gamma: f32) {
        for f in 1..F {
            if !mask[f].is_finite() {
                continue;
            }
            scale_mat(&mut self.acc[f], gamma);
            rank1_acc(&mut self.acc[f], &x[f], mask[f]);
            self.wsum[f] = gamma * self.wsum[f] + mask[f];
        }
    }

    fn phi(&self, f: usize) -> Mat8 {
        let mut a = self.acc[f];
        scale_mat(&mut a, 1.0 / self.wsum[f].max(1e-3));
        a
    }

    /// Apply beamformer + post-filter to one frame; append H samples to pcm. Returns nothing; activity is
    /// computed by the caller from the masks.
    pub fn apply(&mut self, x: &Frame, p: &[f32], post: &[f32], phi_n_diag: &[f32], prm: &SepParams) {
        for f in 0..F {
            let s = if self.have_w { dot_h(&self.w[f], &x[f]) } else { dot_h(&self.steer[f], &x[f]) / prm.m_eff };
            // Wiener post-filter, decision-directed a-priori SNR; noise = wᴴ Φn w (precomputed per bin)
            let nn = phi_n_diag[f].max(1e-14);
            let g_post = s.norm_sqr() / nn;
            let xi = 0.98 * self.pf_prev[f] + 0.02 * (g_post - 1.0).max(0.0);
            let g = (xi / (1.0 + xi)).max(prm.pf_gmin);
            self.pf_prev[f] = g * g * g_post;
            // spatial post-mask: in speech bins, attenuate what the posterior gives to other talkers
            let gs = 1.0 - p[f] + p[f] * post[f].max(prm.sp_floor);
            self.spec[f] = s * (g * gs);
        }
        self.synth.push(&self.spec, &mut self.pcm);
    }

    pub fn noise_out_power(&self, phi_n: &Mat8, f: usize, m_eff: f32) -> f32 {
        if self.have_w { quad(phi_n, &self.w[f]) } else { quad(phi_n, &self.steer[f]) / (m_eff * m_eff) }
    }
}

/// Posterior over streams (+ background) for each bin, in the noise-whitened domain; returns post[c][f].
pub fn posteriors(xw: &Frame, streams: &[Stream], prm: &SepParams) -> Vec<Vec<f32>> {
    let c = streams.len();
    let mut post = vec![vec![0.0f32; F]; c];
    let mut s = vec![0.0f32; c];
    for f in 0..F {
        let nrm = xw[f].iter().map(|v| v.norm_sqr()).sum::<f32>();
        if nrm <= 0.0 {
            continue;
        }
        let mut mx = prm.s_bg;
        for (i, st) in streams.iter().enumerate() {
            s[i] = dot_h(&st.steer_w[f], &xw[f]).norm_sqr() / (st.steer_wn[f] * nrm);
            mx = mx.max(s[i]);
        }
        let mut z = (prm.kappa * (prm.s_bg - mx)).exp();
        for i in 0..c {
            s[i] = (prm.kappa * (s[i] - mx)).exp();
            z += s[i];
        }
        for i in 0..c {
            post[i][f] = s[i] / z;
        }
    }
    post
}

/// Re-solve MVDR weights for all streams: w_c = Φ_c,in⁻¹ d_c / (d_cᴴ Φ_c,in⁻¹ d_c),
/// Φ_c,in = Φn + Σ_{c'≠c} Φ_c'.
pub fn solve_all(streams: &mut [Stream], phi_n: &[Mat8], prm: &SepParams) {
    let c = streams.len();
    for f in 1..F {
        let phis: Vec<Mat8> = streams.iter().map(|s| s.phi(f)).collect();
        for i in 0..c {
            let mut a = phi_n[f];
            for (j, pj) in phis.iter().enumerate() {
                if j != i {
                    for r in 0..M {
                        for q in 0..M {
                            a[r][q] += pj[r][q];
                        }
                    }
                }
            }
            let load = prm.loading * trace_re(&a) / M as f32 + 1e-12;
            let d = streams[i].steer[f];
            if let Some(v) = herm_solve(&a, load, &d) {
                let den = dot_h(&d, &v).re;
                if den > 0.0 && den.is_finite() {
                    let inv = 1.0 / den;
                    streams[i].w[f] = core::array::from_fn(|m| v[m] * inv);
                    continue;
                }
            }
            streams[i].w[f] = core::array::from_fn(|m| d[m] / prm.m_eff);
        }
    }
    for s in streams.iter_mut() {
        s.have_w = true;
    }
}
