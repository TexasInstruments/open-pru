// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Localization + multi-target tracking (impl. note sec. 6).
//! Per time-frequency bin with speech presence, a phase-only (PHAT) steered response picks one direction;
//! bins passing a direct-path coherence test vote into an azimuth histogram (W-disjoint orthogonality of speech).
//! Every `hop` frames the histogram's peaks feed a gated nearest-neighbour tracker. Planar ring => azimuth only.
use crate::common::*;

pub struct Geometry {
    pub mic_xy: Vec<(f32, f32)>,
    pub active: [bool; M],
    pub m_eff: f32,
}

impl Geometry {
    /// Uniform circular array, mic 1 at 0°, counter-clockwise (Scene Simulator layout).
    pub fn ring(radius: f32, active: [bool; M]) -> Self {
        Self {
            active,
            m_eff: active.iter().filter(|&&a| a).count() as f32,
            mic_xy: (0..M)
                .map(|m| {
                    let a = 2.0 * std::f32::consts::PI * m as f32 / M as f32;
                    (radius * a.cos(), radius * a.sin())
                })
                .collect(),
        }
    }
    /// Far-field steering vector for azimuth `az_deg` at bin `f`: a_m = exp(+j 2π f (r_m·u)/c).
    pub fn steer(&self, az_deg: f32, f: usize) -> Vec8 {
        let a = az_deg.to_radians();
        let (ux, uy) = (a.cos(), a.sin());
        let w = 2.0 * std::f32::consts::PI * bin_hz(f) / SPEED_OF_SOUND;
        core::array::from_fn(|m| {
            if !self.active[m] {
                return ZERO;
            }
            let (x, y) = self.mic_xy[m];
            C::from_polar(1.0, w * (x * ux + y * uy))
        })
    }
}

pub struct DoaParams {
    pub f_lo: f32,
    pub f_hi: f32,
    pub step_deg: f32,
    pub p_min: f32,    // speech presence needed for a bin to vote
    pub coh_min: f32,  // direct-path coherence needed
    pub decay: f32,    // per-frame histogram decay
    pub peak_min: f32, // minimum peak strength (fraction of band bins voting)
    pub max_peaks: usize,
    pub min_sep_deg: f32,
}

pub struct Doa {
    prm: DoaParams,
    grid: Vec<f32>,
    bins: Vec<usize>,
    steer: Vec<Vec<Vec8>>, // [band bin][az]
    steer_w: Vec<Vec<Vec8>>, // whitened steering L⁻¹a, [band bin][az]
    steer_wn: Vec<Vec<f32>>, // ‖L⁻¹a‖²
    hist: Vec<f32>,
    m_eff: f32,
    pub stat: [u64; 3],
    pub coh_hist: [u64; 10],
}

impl Doa {
    pub fn new(geo: &Geometry, prm: DoaParams) -> Self {
        let n = (360.0 / prm.step_deg).round() as usize;
        let grid: Vec<f32> = (0..n).map(|i| i as f32 * prm.step_deg).collect();
        let bins: Vec<usize> = (0..F).filter(|&f| bin_hz(f) >= prm.f_lo && bin_hz(f) <= prm.f_hi).collect();
        let steer: Vec<Vec<Vec8>> = bins.iter().map(|&f| grid.iter().map(|&a| geo.steer(a, f)).collect()).collect();
        let steer_w = steer.clone();
        let steer_wn = steer.iter().map(|v| v.iter().map(|a| a.iter().map(|c| c.norm_sqr()).sum()).collect()).collect();
        Self { steer_w, steer_wn, hist: vec![0.0; n], m_eff: geo.m_eff, grid, bins, steer, prm, stat: [0; 3], coh_hist: [0; 10] }
    }

    /// State bytes: steering grid (raw + whitened) over the band bins × azimuth grid, and the vote histogram.
    pub fn mem_bytes(&self) -> usize {
        let g = |v: &Vec<Vec<Vec8>>| v.iter().map(|r| vbytes(r)).sum::<usize>();
        g(&self.steer) + g(&self.steer_w) + self.steer_wn.iter().map(|r| vbytes(r)).sum::<usize>() + vbytes(&self.hist) + vbytes(&self.grid) + vbytes(&self.bins)
    }

    pub fn grid_size(&self) -> (usize, usize) {
        (self.bins.len(), self.grid.len())
    }

    /// Re-whiten the steering grid with the current noise Cholesky factors.
    pub fn refresh_white(&mut self, noise: &crate::noise::NoiseTracker) {
        for (bi, &f) in self.bins.iter().enumerate() {
            let l = &noise.chol_doa[f];
            for (ai, a) in self.steer[bi].iter().enumerate() {
                let aw = fwd_sub(l, a);
                self.steer_wn[bi][ai] = aw.iter().map(|c| c.norm_sqr()).sum::<f32>().max(1e-30);
                self.steer_w[bi][ai] = aw;
            }
        }
    }

    /// Vote with the whitened frame `xw` (L⁻¹x per bin).
    pub fn accumulate(&mut self, xw: &Frame, p: &[f32]) {
        for h in self.hist.iter_mut() {
            *h *= self.prm.decay;
        }
        for (bi, &f) in self.bins.iter().enumerate() {
            self.stat[0] += 1;
            if p[f] < self.prm.p_min {
                continue;
            }
            self.stat[1] += 1;
            let u = &xw[f];
            let nx: f32 = u.iter().map(|c| c.norm_sqr()).sum();
            if nx <= 0.0 {
                continue;
            }
            let (mut best, mut ba) = (0.0f32, 0usize);
            for (ai, a) in self.steer_w[bi].iter().enumerate() {
                let s = dot_h(a, u).norm_sqr() / self.steer_wn[bi][ai];
                if s > best {
                    best = s;
                    ba = ai;
                }
            }
            let coh = best / nx; // cos² between whitened frame and whitened steering, in [0, 1]
            self.coh_hist[((coh * 10.0) as usize).min(9)] += 1;
            if coh >= self.prm.coh_min {
                self.stat[2] += 1;
                self.hist[ba] += p[f];
            }
        }
    }

    /// Peaks of the smoothed histogram: (azimuth deg, strength = fraction of band bins voting).
    pub fn peaks(&self) -> Vec<(f32, f32)> {
        let n = self.hist.len();
        let norm = 1.0 / (self.bins.len() as f32 / (1.0 - self.prm.decay));
        let k: [f32; 7] = [0.05, 0.12, 0.2, 0.26, 0.2, 0.12, 0.05]; // ~±6° at 2° steps
        let sm: Vec<f32> = (0..n)
            .map(|i| (0..7).map(|j| k[j] * self.hist[(i + n + j - 3) % n]).sum::<f32>() * norm)
            .collect();
        let mut cand: Vec<(usize, f32)> =
            (0..n).filter(|&i| sm[i] >= sm[(i + n - 1) % n] && sm[i] > sm[(i + 1) % n] && sm[i] >= self.prm.peak_min).map(|i| (i, sm[i])).collect();
        cand.sort_by(|a, b| b.1.total_cmp(&a.1));
        let mut out: Vec<(f32, f32)> = Vec::new();
        for (i, s) in cand {
            let a = self.grid[i];
            if out.iter().all(|(b, _)| ang_diff(a, *b).abs() >= self.prm.min_sep_deg) {
                out.push((a, s));
                if out.len() == self.prm.max_peaks {
                    break;
                }
            }
        }
        out
    }
}

// ------------------------------------------------------------------ tracker

#[derive(Clone, Debug)]
pub struct Track {
    pub id: usize,
    pub az: f32,
    pub hits: u32,
    pub strong_hits: u32,
    pub confirmed: bool,
    pub born_frame: usize,
    pub last_hit_frame: usize,
    pub strength: f32,
}

pub struct TrackerParams {
    pub gate_deg: f32,
    pub confirm_hits: u32,
    pub tentative_ttl: usize, // frames a tentative track survives without confirmation
    pub confirmed_ttl: usize, // frames a confirmed track survives without hits
    pub birth_min: f32,
    pub max_tracks: usize,
    pub az_alpha: f32,
    pub exclude: Vec<(f32, f32)>, // (azimuth, half-width): known ego-noise directions
}

pub struct Tracker {
    pub prm: TrackerParams,
    pub tracks: Vec<Track>,
    next_id: usize,
}

impl Tracker {
    pub fn new(prm: TrackerParams) -> Self {
        Self { prm, tracks: Vec::new(), next_id: 0 }
    }

    /// Returns ids of tracks confirmed in this update.
    pub fn update(&mut self, frame: usize, peaks: &[(f32, f32)]) -> Vec<usize> {
        let mut used = vec![false; self.tracks.len()];
        let mut newly = Vec::new();
        for &(az, s) in peaks {
            if self.prm.exclude.iter().any(|&(e, w)| ang_diff(az, e).abs() <= w) {
                continue;
            }
            // nearest track inside the gate
            let mut best: Option<(usize, f32)> = None;
            for (i, t) in self.tracks.iter().enumerate() {
                let d = ang_diff(az, t.az).abs();
                if d <= self.prm.gate_deg && !used[i] && best.map_or(true, |(_, bd)| d < bd) {
                    best = Some((i, d));
                }
            }
            match best {
                Some((i, _)) => {
                    used[i] = true;
                    let t = &mut self.tracks[i];
                    let a = if t.confirmed { self.prm.az_alpha } else { 0.5 };
                    t.az = wrap_deg(t.az + a * ang_diff(az, t.az));
                    t.hits += 1;
                    if s >= self.prm.birth_min {
                        t.strong_hits += 1;
                    }
                    t.last_hit_frame = frame;
                    t.strength = 0.7 * t.strength + 0.3 * s;
                    // confirm on repeated STRONG evidence: reflections/sidelobes give many weak hits
                    if !t.confirmed && t.hits >= self.prm.confirm_hits && t.strong_hits >= 2 {
                        t.confirmed = true;
                        newly.push(t.id);
                    }
                }
                None => {
                    // a peak near (but outside the gate of) a used track is a sidelobe: ignore
                    let near = self.tracks.iter().any(|t| ang_diff(az, t.az).abs() < self.prm.gate_deg * 1.5);
                    let n_conf = self.tracks.iter().filter(|t| t.confirmed).count();
                    if s >= self.prm.birth_min && !near && n_conf < self.prm.max_tracks {
                        self.tracks.push(Track { id: self.next_id, az, hits: 1, strong_hits: 1, confirmed: false, born_frame: frame, last_hit_frame: frame, strength: s });
                        used.push(true);
                        self.next_id += 1;
                    }
                }
            }
        }
        let prm = &self.prm;
        self.tracks.retain(|t| {
            let ttl = if t.confirmed { prm.confirmed_ttl } else { prm.tentative_ttl };
            frame - t.last_hit_frame <= ttl
        });
        newly
    }
}
